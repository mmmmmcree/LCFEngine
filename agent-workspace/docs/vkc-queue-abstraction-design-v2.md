# vk_core Queue 抽象设计 v2：构造期不可变 DeviceQueue

> 状态：对 `vkc-queue-abstraction-design.md`（下称 v1）的评审改进稿。
>
> 语言标准：C++23。
>
> 核心变更：**mutex 的存在性由 QueuePlan 在 DeviceQueue 构造时一次性决定，
> 而非 `acquireLogicalQueue()` 的运行时累积副作用。**
> 由此删除 v1 的"冻结不变量"（§5.2）运行时纪律，将其转化为类型级 const 保证。

---

## 1. 与 v1 的差异总览

| 维度 | v1 | v2 |
| --- | --- | --- |
| mutex 何时决定 | 第二次 `acquireLogicalQueue()` 时 emplace | 构造参数 `SharingMode`，构造后不可变 |
| 冻结不变量 | 运行时纪律：初始化后不得再 acquire | 不需要：对象构造后即不可变 |
| `acquireLogicalQueue()` | 非 const、有副作用、修改 mapping count | `const`、无副作用、可任意次调用 |
| "mapping ≠ copy" 双语义 | 需要专门文档解释（v1 §7） | 消失：acquire 与 copy 语义等价 |
| §7.2 footgun | 无锁副本被并发使用 → 静默 UB | 仍存在但缩小：见本文 §5 |
| 命名 | PhysicalQueue（待定） | **DeviceQueue**（采纳 v1 §3.2 论证） |
| 测试面 | 需测 mutex 状态迁移（v1 §14.2） | 只需测构造参数 → mutex 存在性 |

v1 中保持不变、v2 完整继承的部分：

- 分层结构（v1 §4）：QueuePlanner → DeviceCore → DeviceQueue → LogicalQueue → QueueContext。
- QueueContext 按逻辑流独立持有 allocator/timeline/lease（v1 §4.6）。
- QueueAccess 作为 RAII token，空/持锁 `unique_lock`（v1 §4.5）。
- 只请求 QueuePlan 实际分配的 queue 数（v1 §9.1）。
- 第一阶段限制 present 与 graphics 同 family（v1 §9.3）。
- 语义边界声明：不承诺单物理 queue 上的真实并行前进性（v1 §10）。

---

## 2. 关键洞察：mapping 数在构造前已知

v1 §9.2 的创建流程本身就要求：

```text
收集全部 requirements -> QueuePlanner 生成 QueuePlan -> create VkDevice -> 创建 DeviceQueue
```

QueuePlan 生成时，每个 `(family, index)` 会承载多少逻辑角色已经完全确定。
既然如此，"第二次 acquire 时 emplace mutex" 是把编译期/规划期已知的信息
推迟到运行时副作用中重建——这正是 v1 需要冻结纪律的唯一原因。

v2 直接把它前移：

```text
QueuePlan 条目: { family, index, roles: [graphics(tag 0), present(tag 1)] }
        |
        v  角色跨 >=2 个 SubmitterTag（见 §4.0）  =>  SharingMode::Shared
DeviceQueue{queue, family, index, SharingMode::Shared}   // mutex 构造即存在，永不改变
```

## 3. 类型形状

```cpp
namespace lcf::vkc {

class DeviceQueue
{
public:
    enum class SharingMode : uint8_t { Exclusive, Shared };

    DeviceQueue(vk::Queue queue, uint32_t family_index, uint32_t queue_index,
                SharingMode mode) noexcept :
        m_queue(queue), m_family_index(family_index), m_queue_index(queue_index)
    {
        if (mode == SharingMode::Shared) { m_mutex_opt.emplace(); }
    }

    DeviceQueue(const DeviceQueue &) = delete;
    DeviceQueue(DeviceQueue &&) = delete;              // 显式 delete，不依赖成员隐式删除
    DeviceQueue & operator=(const DeviceQueue &) = delete;
    DeviceQueue & operator=(DeviceQueue &&) = delete;

    // const、无副作用、可任意次调用——不再有 mapping count
    LogicalQueue acquireLogicalQueue() const noexcept;

private:
    friend class LogicalQueue;

    vk::Queue m_queue;
    uint32_t m_family_index;
    uint32_t m_queue_index;
    mutable std::optional<std::mutex> m_mutex_opt;     // 存在性构造后不变；mutable 仅为锁定
};

class QueueAccess
{
    using UniqueLock = std::unique_lock<std::mutex>;
public:
    QueueAccess(vk::Queue queue, std::optional<std::mutex> & mutex_opt) noexcept :
        m_queue(queue),
        m_lock(mutex_opt ? UniqueLock(*mutex_opt) : UniqueLock{})
    {
    }
    QueueAccess(const QueueAccess &) = delete;
    QueueAccess & operator=(const QueueAccess &) = delete;
    QueueAccess(QueueAccess &&) noexcept = default;
    QueueAccess & operator=(QueueAccess &&) noexcept = default;

    // 不提供隐式 operator vk::Queue —— 防止裸 handle 逃逸出锁的生命周期
    const vk::Queue & getQueue() const noexcept { return m_queue; }
    const vk::Queue * operator->() const noexcept { return &m_queue; }

private:
    vk::Queue m_queue;
    UniqueLock m_lock;
};

class LogicalQueue
{
public:
    LogicalQueue(const LogicalQueue &) = default;
    LogicalQueue & operator=(const LogicalQueue &) = default;

    QueueAccess acquireAccess() const noexcept
    {
        return QueueAccess(m_device_queue_p->m_queue, m_device_queue_p->m_mutex_opt);
    }
    uint32_t getFamilyIndex() const noexcept { return m_device_queue_p->m_family_index; }
    uint32_t getQueueIndex() const noexcept { return m_device_queue_p->m_queue_index; }

private:
    friend class DeviceQueue;
    explicit LogicalQueue(const DeviceQueue & q) noexcept : m_device_queue_p(&q) {}

    const DeviceQueue * m_device_queue_p;
};

inline LogicalQueue DeviceQueue::acquireLogicalQueue() const noexcept
{
    return LogicalQueue(*this);
}

} // namespace lcf::vkc
```

实现顺序注意（修复 v1 原型 Queue.h 的编译问题）：

- 定义顺序必须为 `DeviceQueue`（声明 acquire）→ `QueueAccess` → `LogicalQueue`
  → `DeviceQueue::acquireLogicalQueue()` 的类外定义。任何在被引用类型完整前
  内联定义的方法体都会编译失败（v1 原型 `Queue.h:32`、`Queue.h:54` 均如此）。
- move 操作显式 `= delete`，不依赖 `std::optional<std::mutex>` 成员导致的
  隐式删除（v1 原型 `= default` 实际被隐式 delete，具有误导性）。

## 4. QueuePlanner 输出即真相

### 4.0 并发意图声明：SubmitterTag

关键修正：Vulkan queue external synchronization 是**线程**问题，不是角色
问题。graphics 和 compute 映射到同一条 queue 时，只要都从同一线程提交，
mutex 是纯浪费。正确判定式：

```text
mutex 需要 ⟺ 映射到同一 DeviceQueue 的角色 跨越 ≥2 个提交线程
```

因此每个角色在 `DeviceContextCreateInfo` 中声明两个正交属性：

```cpp
struct QueueRoleInfo {
    SubmitterTag submitter{0};     // 正确性（硬约束）：同 tag = 用户承诺同一
                                   // 线程提交（或已被上层串行化）；违约是 UB，
                                   // debug 构建可用 owner-thread 断言捕获
    bool prefer_dedicated = false; // 性能（软提示）：GPU 独立前进性 / queue
                                   // priority 需求；落空仅损失性能
};

info.setQueueRoleInfo(QueueRole::Graphics, { .submitter = SubmitterTag{0} })
    .setQueueRoleInfo(QueueRole::Compute,  { .submitter = SubmitterTag{0} })
    .setQueueRoleInfo(QueueRole::Present,  { .submitter = SubmitterTag{1} });
```

默认全部 tag 0：单线程引擎自然零锁。005 只需给 present 一个不同 tag。

planner 决策矩阵：

| 同一 family 上的角色 | 有多余 queue index？ | 结果 |
| --- | --- | --- |
| 跨 ≥2 个 tag | 有 | 拆到不同 index（Exclusive，无锁）|
| 跨 ≥2 个 tag | 无 | 同 index + Shared（mutex）|
| 同一 tag | — | 同 index，Exclusive，且**不消耗多余 queue**（留给真正跨线程的角色）|

第三行是双重收益：同线程的 compute fallback 不加锁，也不挤占 present 的
独立 queue 名额。

"多条同 family queue 的唯一目的是多线程提交"对**正确性**成立，对
**性能**不成立——同 family 多 queue 还服务于 GPU 侧独立前进（队头阻塞，
v1 §3.5/§10）和 `pQueuePriorities` 差异化，故保留 `prefer_dedicated`。
分配优先级：先满足跨 tag 拆分（正确性），再满足 prefer_dedicated（性能）。

### 4.1 QueuePlan 输出

```cpp
struct QueueAssignment
{
    uint32_t family_index;
    uint32_t queue_index;
    // shared 由 planner 导出：该 (family, index) 上的角色跨 >=2 个 SubmitterTag
    bool shared;
};

struct QueuePlan
{
    QueueAssignment graphics;
    QueueAssignment present;
    QueueAssignment compute;
    QueueAssignment transfer;
    // per-family requested count = max(assigned index) + 1（继承 v1 §9.1）
};
```

DeviceCore 消费 QueuePlan：

```cpp
// 每个唯一 (family, index) 恰好一个 DeviceQueue，稳定地址
std::vector<std::unique_ptr<DeviceQueue>> m_device_queues;
```

分配优先级（回答 v1 §15 问题 7），按顺序贪心，且遵循 §4.0 决策矩阵
（同 tag 的角色直接共享 index、不加锁、不消耗多余 queue）：

1. graphics → 首个 graphics-capable family 的 index 0。
2. present → 与 graphics 跨 tag 时取同 family 下一个未用 index；耗尽则与
   graphics 共享 + Shared。同 tag 时直接共享 index，Exclusive。
3. compute → 专用 compute family index 0；否则 graphics family 下一个未用
   index；耗尽则共享 graphics。
4. transfer → 专用 transfer family；否则同 compute 规则。

任何一步产生共享时，对应 `QueueAssignment::shared = true`，且**同一物理
queue 的所有 assignment 同时标记**——这一步在 planner 内是纯函数计算，
可单元测试，不涉及任何运行时状态迁移。

## 5. 线程安全契约（v1 §7 的收窄）

v2 中 acquire 与 copy 语义等价（都只是拿到指向不可变 DeviceQueue 的
handle），所以 v1 §7 的 "mapping ≠ copy" 说明整体删除。剩余契约只有一条：

> **Exclusive DeviceQueue 上的所有 LogicalQueue（无论来源）共享单一
> host-owner 契约**：由持有它的 QueueContext / Swapchain 上层结构保证
> 同一时刻只有一个线程调用 `acquireAccess()`。
> **Shared DeviceQueue 无此要求**——任意副本任意线程并发均安全。

与 v1 §7.2 相比 footgun 面缩小的原因：v1 中"是否安全"取决于 acquire 历史
（同一份代码在不同 acquire 顺序下安全性不同）；v2 中它是 QueuePlan 的静态
属性，初始化日志（v1 §14.4）直接可见，且规划器可以在 debug 构建为
Exclusive queue 附加 owner-thread 断言（`std::atomic<std::thread::id>`，
release 下编译掉），把违约从静默 UB 变成确定性 assert。

若未来希望完全消除该契约：将所有 DeviceQueue 构造为 Shared 即可——这只是
planner 的一行策略改动，不需要改任何类型。这也回答了 v1 §15 问题 3：
无锁路径收益约为一次无竞争 mutex fast path（~20ns）对一次微秒级驱动调用，
保留它成本很低（本设计已把复杂度降到一个构造参数），但如果日后觉得不值，
退路是策略级的，不是架构级的。

## 6. 其余决议（对 v1 §15 的回答）

1. **mapping vs copy**：v2 中该区分不复存在，问题消解。
2. **冻结可见性**：不再依赖冻结；`const` 对象 + 构造先于线程发布，由
   C++ 内存模型的正常 happens-before 保证。
3. 见 §5 末段。
4. `optional<mutex>` 优于内嵌 mutex + gate pointer：所有权与语义自明，
   且 Exclusive 时不构造内核对象。维持 v1 结论。
5. **改名 DeviceQueue**：采纳。v1 §3.2 的论证成立，`VkQueue` 从属于
   `VkDevice`，"Physical" 前缀与 `VkPhysicalDevice` 语义冲突。
6. DeviceCore 只拥有 `VkDevice` + allocator + DeviceQueue pool，是根所有者
   而非新聚合层；Render/ComputeDeviceContext 降级为消费者。维持 v1 结论。
7. 见 §4 优先级规则。
8. **present 限 graphics family**：第一阶段合理，维持 v1 §9.3。
9. **SubmissionToken 第一阶段引入**：建议是。理由：事后从裸
   `vk::SemaphoreSubmitInfo` 迁移的 API 破坏面远大于现在就用 thin token
   包裹 timeline (semaphore, value) 的成本；且 005 现有链路（v1 §10）
   已符合 token 语义，无需额外调度器。
10. **QueueAccess 暴露面**：保留显式 `getQueue()` / `operator->()`，
    删除隐式转换 `operator vk::Queue`。scoped `withNativeQueue(callback)`
    仅在出现第三方集成需求时补充。

## 7. 测试计划（替代 v1 §14）

类型语义：

```cpp
static_assert(std::copy_constructible<LogicalQueue>);
static_assert(not std::copy_constructible<QueueAccess>);
static_assert(std::movable<QueueAccess>);
static_assert(not std::movable<DeviceQueue>);
static_assert(not std::copy_constructible<DeviceQueue>);
```

单元测试（比 v1 §14.2 少一半——无状态迁移可测）：

- `SharingMode::Exclusive` → `acquireAccess()` 不加锁（mutex 为空）。
- `SharingMode::Shared` → 两个线程并发 `acquireAccess()` 串行化。
- QueuePlanner 纯函数测试：给定各种 family/queueCount 拓扑，断言输出的
  assignment 与 shared 标记（覆盖 v1 §5.3 的四行典型映射表）。

集成测试：继承 v1 §14.3 全部条目；可观测性继承 v1 §14.4。

## 8. Context 层架构：单一所有者 DeviceContext + 角色视图

### 8.1 现状问题

现有 `RenderDeviceContext`（`context/RenderDeviceContext.h`）同时拥有
`VkDevice`、MemoryAllocator 和全部 QueueContext——"Render" 这个角色名拥有了
全设备资源。005 因此只能在应用层裸取 present queue
（`getDevice().getQueue(family, 1)`），完全绕过任何同步抽象。

### 8.2 目标结构

```text
InstanceContext                          [保持现状]
  拥有: vk::UniqueInstance
        │
        ▼
DeviceContext        ←── 唯一的重所有者（即本文的 DeviceCore）
  拥有: vk::PhysicalDevice（选择结果）
        vk::UniqueDevice
        MemoryAllocator
        QueuePlan（诊断用）
        vector<unique_ptr<DeviceQueue>>   ← 稳定地址 queue pool
  提供: getGraphicsQueue()/getPresentQueue()/... → LogicalQueue（值返回）
        │
        ├──────────────┬──────────────────┐
        ▼              ▼                  ▼
RenderContext      ComputeContext      Swapchain
  非拥有视图         非拥有视图           持有 present QueueContext（自有）
  拥有: graphics/compute/transfer        blit 录制+提交在此流上
        各一个 QueueContext
```

### 8.3 决策

1. **`VkDevice` 所有权从 RenderDeviceContext 上移到 DeviceContext**。
   角色 context 降级为非拥有轻量视图（持 `DeviceContext *`），与 vk_core
   现有 handle/provider 方向一致——回答 v1 §15 问题 6：不是新聚合层，
   而是把现有聚合拆薄。
2. **QueueContext 归逻辑流的使用者所有，不归 DeviceContext**。
   RenderContext 拥有 graphics/compute/transfer 三个；Swapchain 自己拥有
   present QueueContext（构造时从 DeviceContext 取 present LogicalQueue）。
   DeviceContext 发布后完全只读，天然线程安全，且无需知道逻辑流数量。
3. **QueueContext 最小改造**：成员 `vk::Device + vk::Queue` 换成
   `LogicalQueue`；`submit()` 内部经 `acquireAccess()`；
   签名改为 `create(vk::Device, LogicalQueue)`。timeline/allocator/lease 不动。
4. **生命周期**：沿用两段式 `create()` + `std::error_code` 风格，无
   shared_ptr。销毁顺序 = 创建逆序（Swapchain/RenderContext → DeviceContext
   → InstanceContext），由应用栈序保证。DeviceContext 可 move
   （DeviceQueue 在 unique_ptr 中地址稳定）。
5. **创建流程**（v1 §9.2 具体化）：

```cpp
InstanceContext instance;         instance.create(instance_info);
Window window{...};
auto surface = createSurface(instance, window);      // surface 先于 device（v1 §3.4）

DeviceContextCreateInfo info;
info.setPhysicalDeviceSelectInfo(...)
    .setRequiredDeviceExtensionManifest(...)
    .setPresentSurface(surface);                     // WSI requirement 参与 QueuePlan

DeviceContext device_context;
device_context.create(instance.getInstance(), info); // 选设备 → QueuePlan → VkDevice → queue pool

RenderContext render_context;
render_context.create(device_context);               // 建 graphics/compute/transfer QueueContext

wsi::Swapchain swapchain;
swapchain.create(device_context, surface, ...);      // 取 present LogicalQueue，建自有 QueueContext
```

第一阶段若保留 005 的"先 device 后 window"顺序，可沿用 v1 §9.3 受限策略
（present 假定 graphics family + 事后验证），架构不变，QueuePlanner 少一个输入。

### 8.4 多 surface 与无 surface

设计原则：**QueuePlanner 的输入是 WSI requirement（"需要 present 能力"），
不是 surface 对象本身**。surface 只在规划期精确验证（若已存在）和
swapchain 创建期最终确认两个时刻出场。

**无 surface（headless）**：`DeviceContextCreateInfo` 的 present surface
为可选列表（`addPresentSurface()`，零到多个）。列表为空且未 `enableWSI()`
→ 不启用 swapchain 扩展、不分配 present 角色、`getPresentQueue()` 返回
error。RenderContext/ComputeContext 不受影响——WSI 是 DeviceContext 的
可选 requirement，不是渲染路径的固有前提。

**多 surface（多窗口）**：planner 按 surface 逐个求 present-capable
families，角色合并：

- 存在覆盖全部 surface 的 family（同平台窗口的常态）→ 一个 present
  DeviceQueue，所有 swapchain 共用；
- 否则按覆盖集分配多个 present assignment。

每个 Swapchain 仍拥有独立 present QueueContext；多个 QueueContext 映射同一
DeviceQueue → `Shared` → mutex 串行化——多 swapchain 并发 present 的安全性
由 §5 契约免费获得。physical device 淘汰规则收紧为"必须能 present 到全部
surface"。

**device 创建后才出现的 surface**：§3.3 约束下不能追加 queue，但各平台提供
不依赖 surface 的 presentation support 查询
（如 `vkGetPhysicalDeviceWin32PresentationSupportKHR(family)`），同平台
后续 surface 与已知 surface 的 per-family 能力几乎总是一致。策略：

1. 应用声明 `enableWSI()`（可零 surface）→ planner 用平台级查询预分配
   present 角色；
2. 后续 swapchain 创建时以 `vkGetPhysicalDeviceSurfaceSupportKHR` 验证
   surface 落在预分配 family 内 → 通过则复用 present LogicalQueue；
3. 验证失败（混合 GPU 外接屏等罕见场景）→ 明确 error_code，不静默降级。

边界：预分配 present DeviceQueue 可能按单 swapchain 预期构造为 Exclusive。
处理：`enableWSI()` 时一并声明 present 角色的 SubmitterTag（§4.0）；仅当
present 与既有映射跨 tag 且无多余 queue index 时才 Shared。多个后来
swapchain 若声明同一 present tag（同一提交线程）则共享 Exclusive queue
仍然安全；跨 tag 才需要 Shared 兜底。

三种情况因此是同一条代码路径的三个输入：`[]`（headless）、
`[s0..sn]`（多窗口）、`enableWSI()` + 事后验证（动态窗口）。

## 9. 结论

v1 的方向正确：分层、QueueContext 独立所有权、optional mutex、零动态分配
路径都应保留。v2 的唯一实质变更是**把 mutex 决策从运行时 acquire 副作用
前移到 QueuePlan 驱动的构造参数**，由此：

- 删除冻结不变量这一无法由编译器检查的纪律性约束；
- 消除 "mapping ≠ copy" 双语义及 v1 §7 的大部分解释负担；
- DeviceQueue 成为构造后不可变对象，线程安全论证退化为平凡的
  const-correctness；
- 违反单 owner 契约可在 debug 构建变成确定性断言而非静默 UB；
- 是否保留高端无锁路径降级为 planner 一行策略，不再是架构承诺。

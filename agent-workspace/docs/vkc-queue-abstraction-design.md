# vk_core Queue 抽象与设备上下文分层设计（评审草案）

> 状态：设计讨论中，尚未定案或完整接入。
>
> 语言标准：C++23。
>
> 背景提交：`f24460b9ca503b3f0dea0ed62288f8f629c58890`。
>
> 本文目标：完整描述当前问题、需求、候选设计、性能取舍和待确认边界，
> 供另一个工程师或 LLM 独立评审该方案是否优雅、正确且高效。

---

## 1. 问题来源

`005_hello_static_pipeline` 同时存在两个线程：

- 渲染线程通过 graphics queue 提交渲染命令，并执行 swapchain present 流程。
- 窗口线程在 resize callback 中主动调用 `swapchain.resizeToFit()`。

Swapchain 内部的 `m_present_mutex` 可以串行化 `present()` 与
`resizeToFit()`，但它无法同步另一个对象中的 graphics
`QueueContext::submit()`。

当 graphics 和 present 最终使用同一个 `VkQueue` 时，两个线程可能同时调用：

```text
render thread: vkQueueSubmit2(graphics_queue)
window thread: vkQueueSubmit2/presentKHR(present_queue)
```

如果二者实际是同一个 `VkQueue`，这违反 Vulkan 对 queue host access 的
external synchronization 要求。

提交 `f24460b` 的临时解决方案是：

1. 将每个选中 queue family 的请求数量从 1 改成 2。
2. graphics 使用 queue index 0。
3. present 直接获取同一 graphics family 的 queue index 1。

该方案在 graphics family 至少暴露两条 queue 时可以消除两个线程对同一
`VkQueue` 的竞争，但存在以下问题：

- `queueCount == 1` 的设备无法创建请求两条 queue 的 `VkDevice`。
- 当前实现对每个选中 family 都请求两条 queue，专用 compute/transfer family
  也可能只有一条 queue。
- Swapchain 直接保存裸 `vk::Queue`，可以绕过任何 QueueContext 同步策略。
- 现有 QueueContext 同时承担物理 queue、逻辑提交流、command allocator、
  timeline 和资源回收职责，无法安全表达多个逻辑角色映射到同一物理 queue。

---

## 2. 原始需求

### 2.1 用户侧模型

用户始终按多逻辑队列模型编程：

```text
graphics logical queue
compute logical queue
transfer logical queue
present logical queue
```

用户不需要根据设备能力编写以下分支：

```cpp
if (device_has_multiple_graphics_queues) {
    // 独立 present queue
} else {
    // graphics/present 共享 queue 并手动加锁
}
```

### 2.2 兼容与性能目标

- 高端设备：如果同一 graphics family 有至少两条 queue，graphics 和 present
  映射到不同 `VkQueue`，queue host access 路径不加 mutex。
- 低端设备：如果只有一条 graphics queue，graphics 和 present 映射到同一个
  `VkQueue`，引擎内部自动串行化 host access。
- compute/transfer 没有独立 queue 时，也允许回退到 graphics 或其他兼容 queue。
- 不使用虚函数、运行时继承层次或 `std::variant`。
- 不在每次 submit/present 时动态分配。
- 高端无锁路径只允许一个稳定、可预测的空锁判断。
- LogicalQueue 是可复制的轻量 handle，例如 Swapchain 可以复制
  RenderDeviceContext 提供的 present LogicalQueue。

### 2.3 非目标

- 不承诺在单物理 queue 上获得真实多 queue 的并行吞吐。
- 第一阶段不实现任意依赖图的 queue submission scheduler。
- 第一阶段不支持运行期间动态增加 logical queue mapping。
- 不要求任意两个同一 LogicalQueue 的副本天然可并发调用；该边界见第 7 节。

---

## 3. Vulkan 约束

### 3.1 Queue external synchronization

同一个 `VkQueue` 上的 `vkQueueSubmit*`、`vkQueuePresentKHR`、
`vkQueueWaitIdle`、`vkQueueBindSparse` 等 host 操作必须在应用侧串行化。

同步域属于唯一的 `(VkDevice, familyIndex, queueIndex)`，不是 queue role，也不是
queue family。来自同一 family 的 index 0 和 index 1 是两个不同的 queue host
synchronization domain。

### 3.2 VkQueue 属于 VkDevice

`VkPhysicalDevice` 只提供能力和 queue-family properties。真正的 `VkQueue`：

1. 在 `vkCreateDevice` 时通过 `VkDeviceQueueCreateInfo` 请求。
2. 在 `VkDevice` 创建后通过 family/index 获取。
3. 生命周期从属于该 `VkDevice`。

因此，包装 `vk::Queue` 的对象更准确的名称是 `DeviceQueue`。当前原型使用
`PhysicalQueue`，本文保留该名称讨论，但命名仍是待定项。

### 3.3 Queue 数量创建后不可扩展

`VkDevice` 创建完成后不能再增加：

- requested queue count；
- enabled extensions；
- enabled features。

所有 Render/Compute/Transfer/WSI 需求必须在创建 `VkDevice` 之前合并。

### 3.4 Present 能力依赖 Surface

Present support 是 `(VkPhysicalDevice, queueFamily, VkSurfaceKHR)` 的属性，
不是 physical device 或 queue family 脱离 surface 后的静态能力。

如果要完整选择 present family，surface 必须先于 device queue planning 创建。
当前示例先创建 RenderDeviceContext、后创建 window/surface，只能采用
“假设 graphics family 可 present，随后验证”的受限策略。

### 3.5 物理并行性不能完全隐藏

mutex 可以隐藏 host external synchronization 差异，但不能把一条物理 queue
变成两条真正独立前进的 queue。依赖真实多 queue 同时前进的 wait-before-signal
模式，在 fallback 到同一 queue 时可能失去前进保证。

---

## 4. 候选分层

```text
PhysicalDeviceContext / AdapterInfo
  - VkPhysicalDevice
  - properties / features / memory properties
  - queue-family capabilities
  - per-surface present support
                    |
                    v
DeviceBuilder + QueuePlanner
  - 合并各模块 requirements
  - 选择 physical device
  - 生成 queue family/index 映射
                    |
                    v
DeviceContext / DeviceCore
  - VkDevice
  - MemoryAllocator
  - 稳定地址的 PhysicalQueue/DeviceQueue 对象
                    |
                    v
LogicalQueue
  - 可复制的角色映射 handle
  - 指向一个稳定 PhysicalQueue
                    |
                    v
QueueContext
  - 一个逻辑提交流
  - command allocator / timeline / leases
                    |
                    v
RenderDeviceContext / ComputeDeviceContext / Swapchain
```

### 4.1 PhysicalDeviceContext

`PhysicalDeviceContext` 如果存在，应只描述 adapter 和能力，不应该拥有
`VkDevice` 或 `VkQueue`。否则会混淆 Vulkan 的 physical/logical device 语义。

### 4.2 DeviceContext / DeviceCore

DeviceCore 是一个 `VkDevice` 的共享根所有者，负责：

- `vk::UniqueDevice`；
- MemoryAllocator；
- 根据 QueuePlan 创建的 PhysicalQueue/DeviceQueue；
- 保证 PhysicalQueue 地址稳定；
- 保证 device 晚于所有 queue consumers 销毁。

RenderDeviceContext、ComputeDeviceContext 等不再分别创建 `VkDevice`，而是消费
同一个 DeviceCore 及其预先规划好的 LogicalQueue。

### 4.3 PhysicalQueue / DeviceQueue

一个对象对应一个唯一的实际 queue：

```text
(VkDevice, familyIndex, queueIndex) -> one PhysicalQueue
```

它拥有：

- `vk::Queue` handle；
- family index；
- queue index；
- logical mapping count；
- 可选的 host mutex。

PhysicalQueue 禁止复制和移动，并由 DeviceCore 以稳定地址保存，例如：

```cpp
std::vector<std::unique_ptr<PhysicalQueue>> m_physical_queues;
```

### 4.4 LogicalQueue

LogicalQueue 是轻量、可复制的非 owning handle。它指向 PhysicalQueue，但不拥有
command allocator、timeline 或资源回收队列。

逻辑角色映射和普通 C++ 复制是两个不同概念：

- `PhysicalQueue::acquireLogicalQueue()`：新建一个 logical mapping，增加 mapping count。
- `LogicalQueue copy`：复制同一个 logical mapping 的 handle，不增加 mapping count。

### 4.5 QueueAccess

QueueAccess 是一次 queue host access 的 RAII token：

- 不可复制；
- 可移动；
- 构造时，如果 PhysicalQueue 存在 mutex，则获得 `std::unique_lock`；
- 析构时自动释放；
- mutex 为空时，`unique_lock` 保持空状态。

### 4.6 QueueContext

每一个逻辑提交流必须拥有独立 QueueContext，即使多个 QueueContext 最终映射到
同一个 PhysicalQueue。QueueContext 自己拥有：

- CommandBufferAllocator / command pools；
- TimelineSemaphore 和 target timestamp；
- pending command buffers；
- ResourceLease retirement queue。

不能让 graphics/compute/transfer 直接指向同一个 QueueContext，否则 mutex 只保护
`VkQueue` host call，无法保护 allocator、timeline 和 lease container 的并发访问。

---

## 5. 初始化期映射算法

### 5.1 基本规则

所有映射发生在单线程初始化阶段，并且必须在任何 `acquireAccess()` 之前完成。

```cpp
LogicalQueue PhysicalQueue::acquireLogicalQueue()
{
    ++mapping_count;
    if (mapping_count == 2) {
        host_mutex.emplace();
    }
    return LogicalQueue{*this};
}
```

第一次映射时 mutex 为空。第二次映射时创建 mutex，第三次及以后保持同一 mutex。

第一次返回的 LogicalQueue 只保存 PhysicalQueue 指针，因此第二次映射完成后，
它也会在后续 `acquireAccess()` 中观察到已经创建的 mutex。

### 5.2 冻结不变量

初始化完成并发布 DeviceCore 后：

- 不再调用 `acquireLogicalQueue()`；
- 不再创建或销毁 PhysicalQueue 中的 mutex；
- mapping count 不再改变；
- 运行期只读取 mutex 是否存在。

如果运行期间允许从无锁切换为有锁，会出现已有 QueueAccess 已观察到空 mutex、
新访问却开始使用 mutex 的竞态。单线程初始化 + 发布前冻结是本设计成立的关键。

### 5.3 典型映射

| 设备能力 | Graphics mapping | Present mapping | Physical mutex |
| --- | --- | --- | --- |
| graphics family 有 2 条 queue | `(family, 0)` | `(family, 1)` | 两边均为空 |
| graphics family 只有 1 条 queue | `(family, 0)` | `(family, 0)` | 同一 mutex |
| compute 有专用 family | graphics queue | compute family index 0 | 两边均为空 |
| compute 回退到 graphics | `(graphics, 0)` | `(graphics, 0)` | 同一 mutex |

Swapchain 从 RenderDeviceContext 复制 present LogicalQueue，不是创建新的 logical
mapping，因此不会改变 mapping count。

---

## 6. 候选 C++23 形状

以下代码只表达类型关系和不变量，不是最终接口：

```cpp
class LogicalQueue;

class PhysicalQueue
{
public:
    PhysicalQueue(vk::Queue queue, uint32_t family, uint32_t index) noexcept;

    PhysicalQueue(const PhysicalQueue &) = delete;
    PhysicalQueue(PhysicalQueue &&) = delete;
    PhysicalQueue & operator=(const PhysicalQueue &) = delete;
    PhysicalQueue & operator=(PhysicalQueue &&) = delete;

private:
    friend class QueuePlanner;
    friend class LogicalQueue;

    LogicalQueue acquireLogicalQueue() noexcept;

    vk::Queue m_queue;
    uint32_t m_family_index;
    uint32_t m_queue_index;
    uint32_t m_mapping_count = 0;
    std::optional<std::mutex> m_host_mutex;
};

class QueueAccess
{
public:
    QueueAccess(vk::Queue queue, std::mutex * mutex_p)
        : m_queue(queue)
    {
        if (mutex_p) {
            m_lock = std::unique_lock<std::mutex>{*mutex_p};
        }
    }

    QueueAccess(const QueueAccess &) = delete;
    QueueAccess & operator=(const QueueAccess &) = delete;
    QueueAccess(QueueAccess &&) noexcept = default;
    QueueAccess & operator=(QueueAccess &&) noexcept = default;

    vk::Queue getQueue() const noexcept { return m_queue; }

private:
    vk::Queue m_queue;
    std::unique_lock<std::mutex> m_lock;
};

class LogicalQueue
{
public:
    LogicalQueue(const LogicalQueue &) = default;
    LogicalQueue(LogicalQueue &&) noexcept = default;
    LogicalQueue & operator=(const LogicalQueue &) = default;
    LogicalQueue & operator=(LogicalQueue &&) noexcept = default;

    QueueAccess acquireAccess() const;
    uint32_t getFamilyIndex() const noexcept;
    uint32_t getQueueIndex() const noexcept;

private:
    friend class PhysicalQueue;
    explicit LogicalQueue(PhysicalQueue & queue) noexcept;

    PhysicalQueue * m_physical_queue_p;
};
```

`std::optional<std::mutex>` 的意义是：

- 不需要单独 bool；
- 不需要 `shared_ptr<std::mutex>` 的 control block 和引用计数；
- 不在 QueueAccess 热路径分配；
- mutex 的所有权明确属于 PhysicalQueue；
- PhysicalQueue 本来就必须禁止移动，因此 mutex 不可移动不是额外限制。

QueueAccess 使用 `std::unique_lock`，避免手写裸 mutex 指针的 move 语义导致重复
`unlock()`。

---

## 7. LogicalQueue 复制与线程安全契约

这是当前设计最需要外部评审的边界。

### 7.1 本设计希望保证的情况

低端设备：

```text
GraphicsQueueContext --logical mapping--\
                                      PhysicalQueue(index 0, mutex)
PresentQueueContext  --logical mapping--/
```

两个独立 QueueContext 可以由不同线程调用，因为它们的 QueueAccess 会共享同一
PhysicalQueue mutex。

高端设备：

```text
GraphicsQueueContext -> PhysicalQueue(index 0, no mutex)
PresentQueueContext  -> PhysicalQueue(index 1, no mutex)
```

两个上下文操作不同 `VkQueue`，不需要共享 mutex。

Swapchain 内部可以复制 PresentQueueContext 的 LogicalQueue。Swapchain 自己的
`m_present_mutex` 负责串行化 render thread 的 present 和 window thread 的 resize，
因此该独立 present queue 仍只有一个 host owner。

### 7.2 本设计默认不保证的情况

如果用户把一个无锁 LogicalQueue 任意复制给两个没有上层同步关系的组件，并在
两个线程中同时调用 `acquireAccess()`，copy 本身不会自动把 PhysicalQueue 改成
有锁状态。

这是有意区分：

```text
new logical mapping != C++ handle copy
```

如果 API 要承诺“任意 LogicalQueue 副本都能并发访问”，则所有 PhysicalQueue
都必须始终加锁，或者 copy 必须注册新的并发 owner。后者会破坏冻结设计，并使
copy 成为有副作用、依赖生命周期的重操作。

当前倾向是：LogicalQueue 可复制，但同一个 logical stream 仍有单 host-owner
契约；多逻辑流映射到同一物理 queue 的兼容问题由 PhysicalQueue mutex 解决。

---

## 8. QueueAccess 的使用边界

所有接收 `VkQueue` 的 host 操作必须通过 QueueAccess：

```cpp
auto access = logical_queue.acquireAccess();
access.getQueue().submit2(submit_info);
```

Swapchain 可在一次 access 中覆盖相邻的 submit/present：

```cpp
auto access = present_queue.acquireAccess();
access.getQueue().submit2(blit_submit);
access.getQueue().presentKHR(present_info);
```

锁只覆盖 queue host calls，不覆盖：

- command buffer 录制；
- acquireNextImage；
- swapchain recreate 的全部 CPU 工作；
- fence/timeline 等待；
- 普通资源准备。

应删除或限制不受控的 `getQueue()` 裸 handle 接口，否则调用方可以绕过同步域。
如需第三方 Vulkan 集成，可提供 scoped `withNativeQueue(callback)`。

---

## 9. QueuePlan 与 Device 创建

### 9.1 不应请求所有 queue

不建议无条件请求 physical device 暴露的所有 queues。应只请求 QueuePlan 实际
分配的 family/index：

```cpp
requested_count[family] =
    max(requested_count[family], highest_assigned_index + 1);
```

例如 graphics family 的 `queueCount == 1` 时：

```text
graphics -> index 0
present  -> index 0
requested count = 1
```

`queueCount >= 2` 时：

```text
graphics -> index 0
present  -> index 1
requested count = 2
```

### 9.2 Requirements 必须先合并

推荐创建流程：

```text
Instance
  -> Window/Surface（若需要 WSI）
  -> 收集 Render/Compute/Transfer/WSI requirements
  -> select PhysicalDevice
  -> QueuePlanner 生成 QueuePlan
  -> create VkDevice
  -> 创建稳定 PhysicalQueue pool
  -> 创建各 logical mappings
  -> freeze/publish DeviceCore
  -> 创建 RenderDeviceContext/ComputeDeviceContext/Swapchain
```

### 9.3 Present family 的第一阶段限制

当前 Swapchain 在 present queue 上录制并提交 blit command，因此第一阶段可以
明确限制：present queue 必须来自 graphics family，只在该 family 内决定使用
index 0 还是 index 1。

如果未来允许 present family 与 graphics family 不同，还需要处理：

- present family 是否支持所需 transfer 命令；
- render source image 的 queue-family ownership transfer；
- swapchain image sharing mode 或显式 ownership transfer；
- graphics/present 间 semaphore dependency。

这不是单纯“找到支持 presentKHR 的 family”即可完成的工作。

---

## 10. 隐式同队列的语义边界

用户可以按多个逻辑提交流组织代码，但 fallback 层只能透明隐藏：

- queue handle 是否相同；
- host mutex 是否存在；
- submit/present API 形状。

它不能透明隐藏：

- 实际 GPU 并行吞吐；
- 依赖两个 queue 独立前进的调度算法；
- 任意 wait-before-signal 图在单 queue 上的前进性。

建议后续用引擎自己的 `SubmissionToken` 替代公开裸
`vk::SemaphoreSubmitInfo`。Token 只在 producer submission 已经成功入队后返回，
consumer 才能使用它作为 wait dependency。这样当两个 logical queues 映射到同一
PhysicalQueue 时，不会先提交一个等待尚未入队 signal 的 consumer。

005 当前的链路符合这个约束：

```text
graphics submit
  -> returned completion token
  -> present blit submit
  -> returned completion token
  -> next graphics submit
```

若未来必须支持任意 future timeline wait，则需要真正的 submission scheduler，
不是 mutex 能解决的问题。

---

## 11. 性能判断

### 11.1 高端路径

```text
optional mutex empty
  -> one predictable presence check
  -> empty unique_lock
  -> direct Vulkan queue call
```

没有：

- virtual dispatch；
- `std::variant` visitation；
- shared_ptr refcount；
- submit-time allocation；
- uncontended mutex lock/unlock。

硬件拓扑是运行时信息，因此“有锁/无锁”之间至少需要一个运行时区分。nullable
mutex/optional presence check 是该设计中的最低成本区分。

### 11.2 低端路径

只有映射数大于 1 的 PhysicalQueue 才构造并使用 `std::mutex`。
`std::mutex` 是当前建议的标准设施：

- `std::shared_mutex` 不适合全部为独占操作的 queue host access；
- `std::binary_semaphore` 不保证更快，也缺少 mutex ownership 语义；
- 纯 `atomic_flag` spinlock 不适合耗时不可预测的驱动调用；
- `atomic_flag::wait/notify` 自定义锁只有在 profiling 证明 mutex 是瓶颈后才值得考虑。

实际 queue driver call 的成本通常远高于一次 mutex fast path，但本设计仍按需求
保留高端完全不调用 mutex 的路径。

---

## 12. 当前 Queue.h 原型状态

当前原型已经朝以下方向演进：

- QueueAccess 使用 `std::unique_lock`；
- QueueAccess 不可复制、可移动；
- PhysicalQueue 持有可选 mutex；
- 第二个 logical mapping 创建 mutex；
- LogicalQueue 普通复制不再修改 mapping count。

接入前仍需确认或修复以下机械问题：

- PhysicalQueue 必须在 LogicalQueue 访问其成员前成为完整类型，或把方法定义移到
  PhysicalQueue 定义之后；
- optional 成员必须显式写成 `std::optional<std::mutex>`；
- QueueAccess 不能从 `const std::mutex &` 构造 `unique_lock<std::mutex>`；
- LogicalQueue 的直接构造应为 private，只允许 PhysicalQueue/QueuePlanner 创建新映射；
- PhysicalQueue 必须显式禁止移动；
- PhysicalQueue 必须以稳定地址保存并比所有 LogicalQueue/QueueAccess 长寿；
- 初始化结束后不得再改变 mutex engagement；
- 当前头文件尚未接入 QueueContext/Swapchain，因此主构建尚未覆盖这些类型。

---

## 13. 已考虑的替代方案

| 方案 | 优点 | 问题 | 当前结论 |
| --- | --- | --- | --- |
| 所有 queue 始终使用 mutex | 最简单，任意副本可并发 | 高端路径也有 lock/unlock | 不符合当前性能目标 |
| `shared_ptr<std::mutex>` 放在 LogicalQueue | 副本自然共享 mutex lifetime | mutex 所有权与物理 queue identity 分离；refcount；空锁复制语义仍不等于新 mapping | 不推荐 |
| PhysicalQueue 持有 `optional<mutex>` | 所有权正确、无堆分配、无 refcount | PhysicalQueue 不可移动；依赖初始化冻结 | 当前首选 |
| copy LogicalQueue 时动态启锁 | 看似自动识别共享 | 已开始的无锁 access 无法补锁；copy 产生隐式副作用 | 拒绝 |
| 虚基类 LockingQueue/DirectQueue | 接口统一 | virtual dispatch 和对象层次无必要 | 拒绝 |
| `variant<Direct, Locked>` | 无继承 | 每次访问仍需 dispatch，类型传播复杂 | 拒绝 |
| submission scheduler | 可模拟更多逻辑队列语义 | 复杂度、延迟和状态管理明显增加 | 第一阶段不做 |

---

## 14. 建议验证项目

### 14.1 类型语义

```cpp
static_assert(std::copy_constructible<LogicalQueue>);
static_assert(std::is_copy_assignable_v<LogicalQueue>);
static_assert(not std::copy_constructible<QueueAccess>);
static_assert(std::movable<QueueAccess>);
static_assert(not std::movable<PhysicalQueue>);
```

### 14.2 Queue mapping 单元测试

- 一个 PhysicalQueue acquire 一次：mutex 为空。
- 同一 PhysicalQueue acquire 两次：两个 LogicalQueue 都观察到同一 mutex。
- acquire 三次：不替换 mutex 地址。
- 复制 LogicalQueue：mapping count 不改变。
- 两个不同 PhysicalQueue 各 acquire 一次：互不加锁。

### 14.3 集成测试

- graphics family `queueCount == 1`：005 可运行并频繁 resize，无 validation error。
- graphics family `queueCount >= 2`：graphics index 0、present index 1，二者均无 mutex。
- graphics/compute/transfer 全部回退到一个 queue：并发 submit 无 host access error。
- 独立 compute/transfer queue：不产生不必要 mutex。
- validation layers 开启时反复 resize、最小化、恢复窗口。
- DeviceCore 销毁顺序晚于 Swapchain、QueueContext 和所有 QueueAccess。

### 14.4 可观测性

建议初始化日志输出最终 QueuePlan：

```text
graphics -> family 0, index 0, shared=true
present  -> family 0, index 0, shared=true
compute  -> family 1, index 0, shared=false
transfer -> family 2, index 0, shared=false
```

该信息只用于诊断，不要求用户据此改变编程模型。

---

## 15. 请求外部评审的问题

1. 把 `new logical mapping` 与 `LogicalQueue copy` 明确区分，是否是清晰且可维护的
   API 语义？
2. “所有 mapping 在单线程初始化期完成，随后冻结”是否足以保证 optional mutex
   的并发正确性和可见性？
3. 高端无锁路径是否值得牺牲“任意 LogicalQueue 副本天然线程安全”的更强保证？
4. PhysicalQueue 持有 `optional<mutex>` 是否优于始终内嵌 mutex + nullable gate pointer？
5. PhysicalQueue 是否应改名为 DeviceQueue，以准确表达 `VkQueue` 从属于 VkDevice？
6. DeviceCore + role-specific DeviceContext 的分层是否与 vk_core 现有轻量 handle/provider
   方向一致，还是形成了新的 context 聚合层？
7. QueuePlanner 应如何表达 graphics、present、compute、transfer 的分配优先级，尤其是
   graphics family 只有两条 queue、同时 compute 也需要 fallback 的情况？
8. 第一阶段限制 present queue 与 graphics queue 同 family 是否合理？
9. 是否应在第一阶段同时引入 SubmissionToken，避免 raw semaphore info 隐藏 producer
   queue identity？
10. QueueAccess 是否应该暴露 `vk::Queue`，还是只提供 scoped callback 以防止裸 handle
    逃逸并绕过锁？

---

## 16. 当前倾向的结论

当前首选方案是：

1. DeviceBuilder/QueuePlanner 在创建 VkDevice 前收集完整需求并分配 queue family/index。
2. DeviceCore 创建并稳定持有每个唯一 VkQueue 对应的 PhysicalQueue/DeviceQueue。
3. 不同逻辑角色拥有不同 QueueContext 和 LogicalQueue mapping。
4. 多个 mapping 指向同一 PhysicalQueue 时，初始化期在 PhysicalQueue 内创建 mutex。
5. LogicalQueue 是可复制 handle；copy 不代表新增 mapping。
6. QueueAccess 通过空或持锁的 `std::unique_lock` 隐藏兼容路径。
7. Swapchain 不再保存裸 queue，而是保存 present LogicalQueue 的副本。
8. 高端设备使用独立 graphics/present DeviceQueue，不执行 mutex lock/unlock。
9. 低端设备自动共享 DeviceQueue 并通过 PhysicalQueue mutex 满足 Vulkan external
   synchronization。
10. PhysicalDeviceContext 只描述 adapter；VkDevice、allocator 和实际 queues 由
    DeviceContext/DeviceCore 拥有。

该方案能够隐藏当前 005 暴露的 queue host race，并保留多 queue 设备的直接路径。
它刻意不尝试用一把 mutex 完整模拟多物理 queue 的调度语义；更强的透明性需要后续
SubmissionToken 约束或 submission scheduler。

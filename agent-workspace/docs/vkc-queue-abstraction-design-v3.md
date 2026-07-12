# vk_core Queue 抽象完整设计（v3，统一稿）

> 状态：定稿候选。取代 `vkc-queue-abstraction-design.md`（v1）与
> `vkc-queue-abstraction-design-v2.md`（v2）。
>
> 语言标准：C++23。背景提交：`f24460b`。
>
> 一句话：**用户按 "能力 + 提交线程域" 声明 queue requests，request 即
> key；vk_core 在 device 创建期一次性解析出 family/index 分配与加锁需求，
> 之后一切不可变。上层获得统一的多队列心理模型，真实设备差异被完全屏蔽。**

---

## 1. 设计理念与目标

vk_core 的原则：**只提供 Vulkan 的能力，不替用户做决策。**

由此推出本设计的三条公理：

1. vk_core **不内建** graphics/compute/transfer/present 角色。角色是上层
   语义；vk_core 只接受 `vk::QueueFlags` + surface 等 Vulkan 原语表达的
   能力请求。
2. 用户声明**意图**（需要什么能力、会不会从多个线程提交、是否希望独立
   前进），vk_core 选择**手段**（独立 queue / 映射 / mutex）。手段不进入
   API 表面，只出现在诊断日志。
3. 所有决策在 device 创建期一次完成，产物构造后不可变。没有运行期状态
   迁移，线程安全论证退化为 const-correctness。

性能目标（继承 v1 §2.2）：无虚函数、无 variant、无 shared_ptr、无提交期
分配；拿到独立 `VkQueue` 的路径唯一的运行时开销是一次可预测的空 optional
判断。

非目标（继承 v1 §2.3）：不承诺单物理 queue 上的真实并行吞吐；第一阶段无
submission scheduler；device 创建后不可新增 request（Vulkan §3.3 硬约束）。

---

## 2. 关键判定式

Vulkan queue external synchronization 是**线程**问题，不是角色问题。两条
互相独立的判定：

```text
是否映射到同一 VkQueue  =  独立 queue 数量是否足够（分配问题，见 §6）
是否需要 mutex          =  映射到同一 VkQueue 的 requests 是否跨 ≥2 个提交线程
```

同线程的 requests 折叠到一条 queue：无锁，且不消耗独立 queue 名额。
跨线程但拿到独立 queue：无锁。只有"跨线程且被迫折叠"才有 mutex。

---

## 3. 用户 API

### 3.1 声明期：QueueRequest 与 QueueKey

```cpp
namespace lcf::vkc {

enum class QueueSubmissionThreadTag : uint8_t {};
// 提交线程域。同 tag = 用户承诺不并发调用（同一线程，或已被上层串行化）。
// 违约是 UB，debug 构建以 owner-thread 断言捕获。

struct QueueRequest
{
    vk::QueueFlags required_flags = {};        // 硬约束：family flags 须为其超集
    vk::QueueFlags undesired_flags = {};       // 软偏好：尽量避开含这些位的 family
    vk::SurfaceKHR present_surface = nullptr;  // 硬约束：须 getSurfaceSupportKHR == true
    QueueSubmissionThreadTag submission_thread_tag = {};   // 正确性维度
    float priority = 1.0f;                     // 期望的 queue priority；也参与分配决策
};
// 无 prefer_dedicated："不同 request 在名额允许时自动分散"由算法保证（§6），
// 独立前进的偏好用 undesired_flags + 不同 tag 表达即可。

// 按声明顺序签发连续整数 0..N-1。用户可定义自己的枚举一一对应后 static_cast。
enum class QueueKey : uint32_t {};

class DeviceContextCreateInfo
{
public:
    QueueKey addQueueRequest(const QueueRequest & request) noexcept
    {
        m_queue_requests.push_back(request);
        return QueueKey{static_cast<uint32_t>(m_queue_requests.size() - 1)};
    }
    // 现有 physical device select / extension manifest 接口不变
private:
    std::vector<QueueRequest> m_queue_requests;
};

} // namespace lcf::vkc
```

注意 blit 的规范约束：`vkCmdBlitImage` 的 supported queue types **仅
Graphics**。走 blit 的 present 链路必须 `required_flags = eGraphics`；
`vkCmdCopyImage`（同格式、无缩放）才允许 transfer-capable family。
vk_core 不猜测用户用哪条路径——caps 由用户如实填写。

### 3.2 使用形态

```cpp
enum class AppQueue : uint32_t { Graphics, Compute, Present };  // 上层自己的角色语义

DeviceContextCreateInfo info;
auto k0 = info.addQueueRequest({ .required_flags = vk::QueueFlagBits::eGraphics,
                                 .submission_thread_tag = QueueSubmissionThreadTag{0} });
auto k1 = info.addQueueRequest({ .required_flags = vk::QueueFlagBits::eCompute,
                                 .undesired_flags = vk::QueueFlagBits::eGraphics, // 想要独立 compute family
                                 .submission_thread_tag = QueueSubmissionThreadTag{0} }); // 与 graphics 同线程
auto k2 = info.addQueueRequest({ .required_flags = vk::QueueFlagBits::eGraphics,  // blit 需要
                                 .present_surface = surface.get(),
                                 .submission_thread_tag = QueueSubmissionThreadTag{1} }); // 窗口线程会碰它
// 保证 k0==QueueKey{0}, k1==QueueKey{1}, k2==QueueKey{2}，与 AppQueue 一一对应

DeviceContext device_context;
if (auto ec = device_context.create(instance, info)) { /* ec 指明哪个 key 不可满足 */ }

// DeviceContext 只发 LogicalQueue；Queue 由使用者自建（见 §5）
Queue gfx_queue;
gfx_queue.create(device_context.getDevice(), device_context.getLogicalQueue(k0));

swapchain.create(device_context.getDevice(), device_context.getMemoryAllocator(),
                 device_context.getLogicalQueue(k2), std::move(surface), ...);
// Swapchain 内部自建 present Queue，只依赖 queue/，不依赖 context/
```

用户心理模型自始至终是"我有 N 条队列"。它们是否落在同一 `VkQueue`、有无
锁，不可见（诊断日志除外）。

---

## 4. 物理层类型：DeviceQueue / LogicalQueue / QueueAccess

命名：`VkQueue` 从属 `VkDevice`（v1 §3.2），故为 **DeviceQueue**，不用
PhysicalQueue（与 `VkPhysicalDevice` 语义冲突）。

```cpp
class DeviceQueue
{
public:
    enum class SharingMode : uint8_t { Exclusive, Shared };

    // SharingMode 是构造参数——由 QueuePlan 在构造前算出，构造后永不改变。
    // 这消除了 v1 "第二次 acquire 时 emplace mutex" 的时序耦合与冻结纪律。
    DeviceQueue(vk::Queue queue, uint32_t family_index, uint32_t queue_index,
                SharingMode mode) noexcept :
        m_queue(queue), m_family_index(family_index), m_queue_index(queue_index)
    {
        if (mode == SharingMode::Shared) { m_mutex_opt.emplace(); }
    }

    DeviceQueue(const DeviceQueue &) = delete;
    DeviceQueue(DeviceQueue &&) = delete;              // 显式 delete；地址必须稳定
    DeviceQueue & operator=(const DeviceQueue &) = delete;
    DeviceQueue & operator=(DeviceQueue &&) = delete;

    LogicalQueue acquireLogicalQueue() const noexcept; // const、无副作用、可任意次调用

private:
    friend class LogicalQueue;
    vk::Queue m_queue;
    uint32_t m_family_index;
    uint32_t m_queue_index;
    mutable std::optional<std::mutex> m_mutex_opt;     // 存在性不变；mutable 仅为锁定
};

class QueueAccess    // 一次 queue host access 的 RAII token
{
    using UniqueLock = std::unique_lock<std::mutex>;
public:
    QueueAccess(vk::Queue queue, std::optional<std::mutex> & mutex_opt) noexcept :
        m_queue(queue), m_lock(mutex_opt ? UniqueLock(*mutex_opt) : UniqueLock{}) {}
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

class LogicalQueue   // 轻量可复制 handle；acquire 与 copy 语义等价
{
public:
    LogicalQueue(const LogicalQueue &) = default;
    LogicalQueue & operator=(const LogicalQueue &) = default;

    QueueAccess acquireAccess() const noexcept
    { return QueueAccess(m_device_queue_p->m_queue, m_device_queue_p->m_mutex_opt); }
    uint32_t getFamilyIndex() const noexcept { return m_device_queue_p->m_family_index; }
    uint32_t getQueueIndex() const noexcept { return m_device_queue_p->m_queue_index; }

private:
    friend class DeviceQueue;
    explicit LogicalQueue(const DeviceQueue & q) noexcept : m_device_queue_p(&q) {}
    const DeviceQueue * m_device_queue_p;
};

inline LogicalQueue DeviceQueue::acquireLogicalQueue() const noexcept
{ return LogicalQueue(*this); }
```

实现顺序：`DeviceQueue`（声明 acquire）→ `QueueAccess` → `LogicalQueue` →
`acquireLogicalQueue()` 类外定义。任何在被引用类型完整前内联的定义都无法
编译（现原型 `Queue.h:32`、`Queue.h:54` 即此问题）。move 必须显式
`= delete`，不依赖 `optional<mutex>` 成员导致的隐式删除（现原型
`= default` 有误导性）。

覆盖范围：所有接收 `VkQueue` 的 host 操作（submit2 / presentKHR /
waitIdle / bindSparse）必须经 QueueAccess。锁不覆盖 command 录制、
acquireNextImage、swapchain recreate 的 CPU 工作、fence/timeline 等待。
一次 access 可覆盖相邻 submit + present（Swapchain blit 路径）。

线程安全契约：

> Exclusive DeviceQueue 上的所有 LogicalQueue（无论来源）共享其
> QueueSubmissionThreadTag 的单 owner 契约；Shared DeviceQueue 任意线程并发安全。
> debug 构建为 Exclusive queue 附加 owner-thread 断言
> （`std::atomic<std::thread::id>`，release 编译掉），把声明错 tag 从
> 静默 UB 变为确定性 assert。

---

## 5. 目录布局与 Context 层架构

### 5.0 queue/ 成为自洽的一层

原 `context/QueueContext` 迁移至 `queue/` 并更名为 **`Queue`**——用户面对
的对象就应该叫 Queue，这正是"统一心理队列模型"的字面兑现。三层命名构成
语义阶梯：`DeviceQueue`（物理）→ `LogicalQueue`（映射 handle）→
`Queue`（用户队列）。不用 QueueProxy：它拥有独立资源（timeline /
allocator / lease），并非无状态转发。

```text
queue/
  DeviceQueue.h       物理 queue（optional mutex，构造期定型）
  LogicalQueue.h      轻量映射 handle
  QueueAccess.h       host access RAII token
  Queue.h             用户队列（原 QueueContext）
  SubmissionToken.h   提交回执
  QueuePlanner.h      纯函数分配器（§6）
```

依赖方向：`context/` → `queue/` → `command/`+`sync/`；`WSI/` → `queue/`，
**不再依赖 `context/`**。

### 5.1 DeviceContext：只发 LogicalQueue，不签发 Queue

Queue 的创建能力对用户开放；DeviceContext 只解析 requests 并按 key 提供
LogicalQueue。理由：把 allocator + timeline 打包塞给每个 key 本身就是一个
替用户做的决策——用户可能想让 Swapchain 内部自建 present Queue，或在同一
tag 下建多条提交流。

```text
InstanceContext                          [保持现状]
  拥有: vk::UniqueInstance
        │
        ▼
DeviceContext        ←── 唯一的重所有者；发布后只读，不感知逻辑流数量
  拥有: vk::PhysicalDevice（选择结果）
        vk::UniqueDevice
        MemoryAllocator
        QueuePlan（诊断）
        vector<unique_ptr<DeviceQueue>>     ← 每个唯一 (family,index) 一个，地址稳定
  提供: getLogicalQueue(QueueKey) -> LogicalQueue（值返回，可复制）
        │
        ├──────────────────────┬─────────────────────┐
        ▼                      ▼                     ▼
   上层渲染模块            上层计算模块           Swapchain
   自建 Queue              自建 Queue             自建内部 present Queue
                                                  拥有 vk::UniqueSurfaceKHR（移交所得）
```

```cpp
class DeviceContext
{
public:
    std::error_code create(vk::Instance instance, const DeviceContextCreateInfo & info) noexcept;
    // 返回的 handle 继承该 request 声明的 submission_thread_tag 契约：无论复制多少份、
    // 包进多少个 Queue，"该 tag 不并发调用"由用户对自己的声明负责
    LogicalQueue getLogicalQueue(QueueKey key) const noexcept;
    // getDevice() / getMemoryAllocator() 同现状
};
```

决策记录：

- **`VkDevice` 所有权从 RenderDeviceContext 上移**。原
  RenderDeviceContext 以"Render"角色名拥有全设备资源，导致 005 只能在
  应用层裸取 present queue（`getDevice().getQueue(family, 1)`）。
- **Queue 由使用者自建**，多 Queue 复用同一 LogicalQueue 合法（各自资源
  独立，安全契约停留在 tag 层，无需任何新机制）。
- **销毁顺序 = 创建逆序**（各 Queue/Swapchain → DeviceContext →
  InstanceContext），由应用栈序保证。DeviceContext 可 move（内容物均在
  unique_ptr 中，地址稳定）。
- **QueueKey 误用面**：连续整数枚举，唯一误用是拿 A 设备的 key 查 B 设备；
  debug 构建越界/序号不匹配时断言。

### 5.2 Queue（原 QueueContext，每实例独占一套资源）

```cpp
class Queue
{
public:
    Queue(const Queue &) = delete;      // 不可复制不可移动，地址稳定
    Queue(Queue &&) = delete;

    std::error_code create(vk::Device device, LogicalQueue logical_queue) noexcept;

    const uint32_t & getFamilyIndex() const noexcept;
    std::expected<CommandBufferBatch, std::error_code>
        allocateCommandBufferBatch(const CommandBufferAllocateInfo &) noexcept;
    std::expected<SubmissionToken, std::error_code> submit(CommandBufferBatch &&) noexcept;
    void collectGarbage() noexcept;

private:
    LogicalQueue m_logical_queue;                     // 唯一物理接触点；submit 内走 acquireAccess()
    TimelineSemaphore m_timeline;                     // 独占
    details::CommandBufferAllocator m_cmd_allocator;  // 独占
    LeaseBatchQueue m_lease_batch_queue;              // 独占
};
```

相对现状的结构变化：`vk::Device + vk::Queue + family_index` 坍缩为
`LogicalQueue`；公开 `getQueue()` 删除。两个同 tag 的 Queue 折叠到
一条 `VkQueue` 时，timeline/allocator/lease 天然隔离，只有 host call 交错
——mutex 只需保护 queue host call 的不变量（v1 §4.6）自动满足。

`submit()` 返回 **SubmissionToken**（包裹 timeline semaphore + value），
替代裸 `vk::SemaphoreSubmitInfo`：token 只在 producer 提交成功入队后存在，
consumer 以 token 表达 wait 依赖，天然避免"先提交等待未入队 signal 的
consumer"在单物理 queue 上失去前进性（v1 §10）。第一阶段即引入——事后
迁移的 API 破坏面远大于现在包一层的成本。

---

## 6. 分配算法：solve_queue_requests

规模极小（request ≤ ~8，family ≤ ~6），采用**精确回溯搜索**：确定性、可证
最优、初始化期成本可忽略。两层结构：

- **外层（DFS 搜索）**：枚举每个 request 落在哪个 family；
- **内层（闭式计算）**：request 落定 family 后，family 内的 queue index
  分配直接算出，不参与搜索。

指导原则（词典序，自上而下）：

1. 不加不必要的锁——跨 tag 的 request 尽量拆到不同 queue；
2. 尊重 undesired_flags——尽量落在 best-fit family；
3. **不同 request 尽量不同队列**（signature 不同、名额允许时自动分散）；
4. **相同 request 尽量同队列**（signature 相同永不拆散），兼省驱动资源。

其中 signature = (required_flags, undesired_flags, present_surface, tag)，
即"相同 request"的判据；priority 不参与 signature（同 signature 不同
priority 仍同队列，queue priority 取 max）。

### 6.1 类型定义

```cpp
namespace lcf::vkc {

// ---- 输出：每个唯一 (family_index, queue_index) 一条 ----
struct DeviceQueueInfo
{
    uint32_t family_index = 0;
    uint32_t queue_index = 0;
    std::vector<uint32_t> request_indices;   // 映射到本条 queue 的 requests（声明序）
    DeviceQueue::SharingMode sharing_mode = DeviceQueue::SharingMode::Exclusive;
    float queue_priority = 1.0f;             // pQueuePriorities = max(成员 priority)
};

namespace details {

struct FamilyInfo
{
    vk::QueueFlags effective_flags = {};  // graphics/compute family 隐含 transfer（规范）
    uint32_t queue_count = 0;
    std::vector<bool> present_support;    // 按 request 下标；无 surface 的 request 恒 true
};

struct CandidateSet                       // 某 request 的候选 family，best-fit 段在前
{
    std::vector<uint32_t> family_indices; // [0, best_fit_count) 不含 undesired 位
    uint32_t best_fit_count = 0;          // 其后为 fit 段（满足硬约束但含 undesired 位）
};

// 词典序代价：operator<=> 按成员声明序比较，越小越好。
// C1/C3 用 Σ(每 queue 的折叠次数) 而非 "Shared queue 条数"——前者与折叠的
// 分布方式无关（4 个 tag 塞进 2 条 queue，无论 3+1 还是 2+2 都记 2 次），
// 把 "怎么分布" 留给叶子算法做负载/优先级权衡，代价函数只管 "折叠了几次"。
struct Cost
{
    uint32_t forced_share_count = 0;    // C1: Σ_queue (驻留 tag 数 - 1)，跨线程折叠次数 → mutex
    uint32_t fit_fallback_count = 0;    // C2: 落在 fit 段 family 的 request 数
    uint32_t mixed_signature_count = 0; // C3: Σ_queue (signature 数 - tag 数)，同线程异质折叠次数
    uint32_t total_queue_count = 0;     // C4: VkQueue 总数（相同 request 聚合 + 省资源）
    auto operator<=>(const Cost &) const = default;
};

struct EvaluatedPlan
{
    std::vector<DeviceQueueInfo> device_queue_infos;
    Cost cost;
};

} // namespace lcf::vkc::details
} // namespace lcf::vkc
```

### 6.2 函数签名

```cpp
namespace lcf::vkc {

// 顶层：纯函数，同输入永远同输出。
// 不可满足时返回 error（日志指明具体 request index 与缺失能力）。
std::expected<std::vector<DeviceQueueInfo>, std::error_code>
solve_queue_requests(vk::PhysicalDevice physical_device,
                     std::span<const QueueRequest> requests) noexcept;

namespace details {

// ① 采集 family 能力：effective flags 折算、queue count、逐 request 的 present 支持
std::vector<FamilyInfo>
collect_family_infos(vk::PhysicalDevice physical_device,
                     std::span<const QueueRequest> requests) noexcept;

// ② 每个 request 的候选 family：硬过滤（required + surface），软分段（undesired）
std::expected<std::vector<CandidateSet>, std::error_code>
compute_candidate_sets(std::span<const FamilyInfo> families,
                       std::span<const QueueRequest> requests) noexcept;

// ③ 搜索顺序：最受约束优先
std::vector<uint32_t>
make_search_order(std::span<const CandidateSet> candidates,
                  std::span<const QueueRequest> requests) noexcept;

// ④ 内层闭式计算：单 family 内的 queue index 分配
std::vector<DeviceQueueInfo>
assign_queue_indices(uint32_t family_index,
                     std::span<const uint32_t> request_ids,   // 落入本 family 的 requests
                     std::span<const QueueRequest> requests,
                     uint32_t queue_count) noexcept;

// ⑤ 叶子求值：完整 request→family 映射 → 逐 family 调 ④，汇总 Cost
EvaluatedPlan
evaluate_assignment(std::span<const uint32_t> family_of_request,
                    std::span<const QueueRequest> requests,
                    std::span<const FamilyInfo> families,
                    std::span<const CandidateSet> candidates) noexcept;

} // namespace lcf::vkc::details
} // namespace lcf::vkc
```

### 6.3 函数实现

```cpp
namespace lcf::vkc::details {

std::vector<FamilyInfo>
collect_family_infos(vk::PhysicalDevice physical_device,
                     std::span<const QueueRequest> requests) noexcept
{
    auto properties = physical_device.getQueueFamilyProperties();
    std::vector<FamilyInfo> families(properties.size());
    for (uint32_t f = 0; f < properties.size(); ++f) {
        auto flags = properties[f].queueFlags;
        // Vulkan 规范：graphics / compute family 隐含支持 transfer（即使未报告该位）
        if (flags & (vk::QueueFlagBits::eGraphics | vk::QueueFlagBits::eCompute)) {
            flags |= vk::QueueFlagBits::eTransfer;
        }
        families[f].effective_flags = flags;
        families[f].queue_count = properties[f].queueCount;
        families[f].present_support.resize(requests.size());
        for (uint32_t i = 0; i < requests.size(); ++i) {
            families[f].present_support[i] = !requests[i].present_surface
                || physical_device.getSurfaceSupportKHR(f, requests[i].present_surface);
        }
    }
    return families;
}

std::expected<std::vector<CandidateSet>, std::error_code>
compute_candidate_sets(std::span<const FamilyInfo> families,
                       std::span<const QueueRequest> requests) noexcept
{
    std::vector<CandidateSet> candidates(requests.size());
    for (uint32_t i = 0; i < requests.size(); ++i) {
        std::vector<uint32_t> fit;
        for (uint32_t f = 0; f < families.size(); ++f) {
            bool caps_ok = (families[f].effective_flags & requests[i].required_flags)
                           == requests[i].required_flags;
            if (!caps_ok || !families[f].present_support[i]) { continue; }  // 硬过滤
            if (families[f].effective_flags & requests[i].undesired_flags) {
                fit.push_back(f);                             // 可用但含 undesired 位
            } else {
                candidates[i].family_indices.push_back(f);    // best-fit 段
            }
        }
        candidates[i].best_fit_count =
            static_cast<uint32_t>(candidates[i].family_indices.size());
        candidates[i].family_indices.append_range(fit);
        if (candidates[i].family_indices.empty()) {
            // 日志指明 request i 与缺失能力，供设备淘汰或用户修正 request
            return std::unexpected(make_error_code(Errc::UnsatisfiableQueueRequest));
        }
    }
    return candidates;
}

std::vector<uint32_t>
make_search_order(std::span<const CandidateSet> candidates,
                  std::span<const QueueRequest> requests) noexcept
{
    std::vector<uint32_t> order(requests.size());
    std::iota(order.begin(), order.end(), 0u);
    // 最受约束优先（候选少者先落位，剪枝最快），同约束度内高 priority 优先，
    // 最后以声明序打破并列（确定性）
    std::ranges::sort(order, [&](uint32_t a, uint32_t b) {
        return std::tuple{candidates[a].family_indices.size(), -requests[a].priority, a}
             < std::tuple{candidates[b].family_indices.size(), -requests[b].priority, b};
    });
    return order;
}

std::vector<DeviceQueueInfo>
assign_queue_indices(uint32_t family_index,
                     std::span<const uint32_t> request_ids,
                     std::span<const QueueRequest> requests,
                     uint32_t queue_count) noexcept
{
    // signature 子组是原子单元：完全相同的 request 永远同队列（原则 4）。
    // tag 组是正确性边界：跨 tag 折叠才需要 mutex（原则 1）。
    struct SignatureGroup { std::vector<uint32_t> members; };
    struct TagGroup
    {
        QueueSubmissionThreadTag tag;
        std::vector<SignatureGroup> sub_groups;   // 按首个成员声明序
        float max_priority = 0.0f;
        uint32_t member_count = 0;
    };

    // ---- 分组（request_ids 按声明序传入，线性查找保持稳定序；N 极小） ----
    std::vector<TagGroup> tag_groups;
    for (uint32_t id : request_ids) {
        TagGroup & tg = find_or_append_tag(tag_groups, requests[id].submission_thread_tag);
        SignatureGroup & sg = find_or_append_signature(tg, requests, id);  // 按四元组匹配
        sg.members.push_back(id);
        tg.max_priority = std::max(tg.max_priority, requests[id].priority);
        ++tg.member_count;
    }
    // 高优先级组先拿独立 index；并列时大组优先；再并列按首个成员声明序
    std::ranges::sort(tag_groups, [](const TagGroup & a, const TagGroup & b) {
        return std::tuple{-a.max_priority, ~uint64_t{a.member_count}, a.sub_groups[0].members[0]}
             < std::tuple{-b.max_priority, ~uint64_t{b.member_count}, b.sub_groups[0].members[0]};
    });

    struct WorkingQueue
    {
        std::vector<SignatureGroup> sub_groups;
        uint32_t tag_count = 1;
        float max_priority = 0.0f;
    };
    std::vector<WorkingQueue> queues;

    // ---- ① 每个 tag 组占一个独立 index；溢出组并入 max_priority 最小的 queue ----
    // 并入目标选 max_priority 最小者：让高优先级 queue 尽量保持无锁独占
    //（热渲染循环的 submit 走 fast path，争用集中到低优先级 queue）。
    // 不变量：溢出与空余 index 不会同时存在（溢出 ⟹ queues.size()==queue_count）。
    for (TagGroup & tg : tag_groups) {
        if (queues.size() < queue_count) {
            queues.push_back({std::move(tg.sub_groups), 1u, tg.max_priority});
        } else {
            WorkingQueue & q = *std::ranges::min_element(queues, {}, &WorkingQueue::max_priority);
            q.sub_groups.append_range(std::move(tg.sub_groups));
            q.max_priority = std::max(q.max_priority, tg.max_priority);
            ++q.tag_count;                                    // ≥2 → Shared
        }
    }

    // ---- ② 空余 index 用于分散：不同 signature 尽量不同队列（原则 3） ----
    // ① 的不变量保证此处所有 queue 都是单 tag（Exclusive），拆分不影响 C1。
    // 同 tag 分布在多条 queue 不需要锁——tag 约束"不并发调用"，与 queue 条数无关。
    while (queues.size() < queue_count) {
        auto it = std::ranges::max_element(queues, {},
            [](const WorkingQueue & q) { return q.sub_groups.size(); });
        if (it->sub_groups.size() < 2) { break; }             // 每条 queue 已单一 signature
        queues.push_back({{std::move(it->sub_groups.back())}, 1u, 0.0f});
        it->sub_groups.pop_back();                            // max_priority 在 ③ 统一重算
    }

    // ---- ③ 汇总 ----
    std::vector<DeviceQueueInfo> result;
    result.reserve(queues.size());
    for (uint32_t index = 0; index < queues.size(); ++index) {
        DeviceQueueInfo info{.family_index = family_index, .queue_index = index};
        for (SignatureGroup & sg : queues[index].sub_groups) {
            info.request_indices.append_range(std::move(sg.members));
        }
        std::ranges::sort(info.request_indices);              // 恢复声明序
        info.sharing_mode = queues[index].tag_count >= 2
            ? DeviceQueue::SharingMode::Shared
            : DeviceQueue::SharingMode::Exclusive;
        info.queue_priority = std::ranges::max(info.request_indices
            | std::views::transform([&](uint32_t id) { return requests[id].priority; }));
        result.push_back(std::move(info));
    }
    return result;
}

EvaluatedPlan
evaluate_assignment(std::span<const uint32_t> family_of_request,
                    std::span<const QueueRequest> requests,
                    std::span<const FamilyInfo> families,
                    std::span<const CandidateSet> candidates) noexcept
{
    EvaluatedPlan plan;
    // 按 family 归桶（i 递增 → 桶内保持声明序）
    std::vector<std::vector<uint32_t>> per_family(families.size());
    for (uint32_t i = 0; i < requests.size(); ++i) {
        per_family[family_of_request[i]].push_back(i);
    }
    for (uint32_t f = 0; f < families.size(); ++f) {
        if (per_family[f].empty()) { continue; }
        auto queues = assign_queue_indices(f, per_family[f], requests, families[f].queue_count);
        for (const DeviceQueueInfo & q : queues) {
            uint32_t tags = count_distinct_tags(q.request_indices, requests);
            uint32_t sigs = count_distinct_signatures(q.request_indices, requests);
            plan.cost.forced_share_count    += tags - 1;      // C1
            plan.cost.mixed_signature_count += sigs - tags;   // C3
        }
        plan.cost.total_queue_count += static_cast<uint32_t>(queues.size());  // C4
        plan.device_queue_infos.append_range(std::move(queues));
    }
    for (uint32_t i = 0; i < requests.size(); ++i) {          // C2
        const CandidateSet & c = candidates[i];
        auto pos = std::ranges::find(c.family_indices, family_of_request[i])
                 - c.family_indices.begin();
        plan.cost.fit_fallback_count += (pos >= c.best_fit_count);
    }
    return plan;
}

} // namespace lcf::vkc::details

namespace lcf::vkc {

std::expected<std::vector<DeviceQueueInfo>, std::error_code>
solve_queue_requests(vk::PhysicalDevice physical_device,
                     std::span<const QueueRequest> requests) noexcept
{
    using namespace details;
    auto families = collect_family_infos(physical_device, requests);
    auto candidates = compute_candidate_sets(families, requests);
    if (!candidates) { return std::unexpected(candidates.error()); }
    auto order = make_search_order(*candidates, requests);

    std::vector<uint32_t> family_of_request(requests.size());
    std::optional<EvaluatedPlan> best;

    // 穷举 DFS。空间 = Π|candidates(i)|，真实设备远小于理论上界。
    // 首个叶子即"最受约束优先 + best-fit 优先"的贪心解，之后只会更优。
    // 若 request 数增长到可感知，可加分支限界（对部分分配统计 C1 下界剪枝）。
    auto dfs = [&](this auto && self, size_t depth) -> void {
        if (depth == order.size()) {
            auto plan = evaluate_assignment(family_of_request, requests, families, *candidates);
            if (!best || plan.cost < best->cost) { best = std::move(plan); }
            return;
        }
        for (uint32_t f : (*candidates)[order[depth]].family_indices) {
            family_of_request[order[depth]] = f;
            self(depth + 1);
        }
    };
    dfs(0);
    return std::move(best->device_queue_infos);   // candidates 全非空 ⟹ best 必有值
}

} // namespace lcf::vkc
```

### 6.4 测试用例

统一使用如下 request 集（除非注明），tag 全部互异模拟多线程引擎：

```text
R0 主渲染:  required = Graphics                        tag 0  pri 1.0
R1 present: required = Graphics, surface = s0          tag 1  pri 0.9
R2 异步计算: required = Compute,  undesired = Graphics  tag 2  pri 0.8
R3 上传:    required = Transfer, undesired = G|C       tag 3  pri 0.5
```

**用例 A：真实桌面 GPU**（本机拓扑）

```text
family 0: G|C|T|SB       ×16  (present 支持)
family 1: T|SB           ×2
family 2: C|T|SB         ×8
family 3: T|SB|Video...  ×2   family 4: ×2   family 5: T|SB|OF ×1
```

候选：R0→best{0}；R1→best{0}；R2→best{2}, fit{0}；R3→best{1,3,4,5}, fit{0,2}。
期望结果：

```text
R0 -> (family 0, queue 0)  Exclusive     R2 -> (family 2, queue 0)  Exclusive
R1 -> (family 0, queue 1)  Exclusive     R3 -> (family 1, queue 0)  Exclusive
Cost = {0, 0, 0, 4}    全部独立、零锁、undesired 全部避开
```

原则检验：四个互不相同的 request → 四条不同 queue ✔。

**用例 B：相同 request 聚合**。在 A 基础上追加 R4 = R0 的完整复制
（signature 同，pri 0.7）：R4 与 R0 同为 fam0 tag0 的同一 signature 子组
→ 同队列 (0,0)，queue_priority = 1.0。②阶段永不拆分单 signature 队列，
即使还有 14 条空 queue ✔（原则 4）。

**用例 C：极端单队列设备**：仅 family 0 = G|C|T ×1。
R0..R3 全落 fam0（R2/R3 走 fit 段）。4 个 tag 组、1 条 queue：R0（最高
pri）先占，R1/R2/R3 依次并入 → 一条 Shared queue。

```text
Cost = {C1=3, C2=2, C3=0, C4=1}    一个 mutex 保护一切，正确退化 ✔
```

**用例 D：G|C|T ×2（单 family 双 queue）**，同 C 的 requests：
① R0→q0，R1→q1；R2、R3 溢出，均并入 max_priority 较小的 q1。

```text
q0 {R0}          Exclusive  ← 主渲染保持无锁 fast path
q1 {R1,R2,R3}    Shared     ← 争用集中到低优先级 queue
Cost = {C1=2, C2=2, C3=0, C4=2}
```

体现溢出并入规则"保护高优先级 queue"的意图。

**用例 E：同 tag 异质的分散与回退**。仅 family 0 = G|C|T，requests：
gfx(G, tag0, 1.0)、comp(C, tag0, 0.8)、pres(G+surface, tag1, 0.9)。

```text
queue_count = 3: ① tag0→q0, tag1→q1  ② 空余 q2：从 q0 拆出 comp 子组
    → gfx q0 / pres q1 / comp q2，全 Exclusive，Cost={0,*,0,3}   ✔ 原则 3
queue_count = 2: ② 无空余 → gfx+comp 同 q0（Exclusive，无锁！），pres q1
    → Cost={0,*,1,2}   同线程折叠优于加锁 ✔ §2 判定式
queue_count = 1: 全折叠 q0 Shared，Cost={1,*,1,1}   ✔ 逐级退化
```

**用例 F：回溯优于贪心的构造**。fam0 = G|T ×1（present 支持），
fam1 = C|T ×1；requests：gfx(G, tag0, 1.0)→{0}、comp(C, tag1, 0.9)→{1}、
xfer(T, undesired={}, tag1, 0.5)→{0,1}（与 comp 同 tag）。

```text
贪心（首叶）: xfer 按候选序落 fam0 → 与 gfx 跨 tag 折叠 → C1=1
回溯: 尝试 xfer→fam1 → 与 comp 同 tag 折叠（无锁）→ Cost={0,0,1,2} 更优 ✔
```

这是保留搜索层的全部理由：跨 family 的容量/tag 互动，贪心的局部视角
看不见。

### 6.5 可观测性

初始化日志输出最终 plan（仅诊断，不要求用户据此改变编程模型）：

```text
request 0 -> family 0, queue 0, exclusive, priority 1.0   (tag 0)
request 1 -> family 0, queue 1, exclusive, priority 0.9   (tag 1)
request 2 -> family 2, queue 0, exclusive, priority 0.8   (tag 2)
request 3 -> family 1, queue 0, exclusive, priority 0.5   (tag 3)
```

---

## 7. WSI：surface 生命周期与多/无/晚到 surface

### 7.1 Surface 所有权链

```text
createSurface(instance, window) -> vk::UniqueSurfaceKHR
        │  借用 raw handle 填入 QueueRequest::present_surface（规划期只读）
        ▼
swapchain.create(device, allocator, logical_queue, std::move(unique_surface), ...)
        │  UniqueSurface 移交 Swapchain，此后由 Swapchain 管理
        ▼
Swapchain 拥有 surface，并以传入的 LogicalQueue 内部自建 present Queue，
不再保存任何裸 vk::Queue，也不依赖 context/
```

surface 必须先于 DeviceContext 创建（present support 是
`(physicalDevice, family, surface)` 属性，v1 §3.4）；销毁顺序上
Swapchain（连同 surface）先于 DeviceContext，先于 InstanceContext。

### 7.2 无 surface（headless）

无含 surface 的 request → 不启用 swapchain 扩展、不分配 present 能力。
其余 request 完全不受影响：WSI 是 requests 集合的一个可选成员，不是渲染
路径的固有前提。

### 7.3 多 surface（多窗口）

每个 surface 一个含 `present_surface` 的 request（各自的 tag 如实声明）。
分配算法自然处理：同 tag 的多个 present request 可折叠（无锁）；跨 tag
且 queue 不足时折叠为 Shared；queue 充足时各自独立。physical device 淘汰
条件"覆盖所有 request"自动涵盖"能 present 到所有 surface"。

### 7.4 device 创建后才出现的 surface

Vulkan 不允许追加 queue，但各平台提供不依赖 surface 的 presentation
support 查询（如 `vkGetPhysicalDeviceWin32PresentationSupportKHR(family)`），
同平台后续 surface 与已知 surface 的 per-family 能力几乎总是一致：

1. 预声明一个 `present_surface = nullptr` 但带 `expects_presentation`
   语义的 request（以平台级查询做候选判定），tag 如实声明；
2. 晚到的 swapchain 创建时以 `vkGetPhysicalDeviceSurfaceSupportKHR` 验证
   surface 落在已分配 family 内 → 通过则复用该 key；
3. 验证失败（混合 GPU 外接屏等罕见场景）→ 明确 error_code，不静默降级。

多个晚到 swapchain 若同 tag（同一提交线程）→ 共享该 Exclusive queue 依然
安全；确有跨线程需求则预声明时就用不同 tag 的两个 request。

---

## 8. 语义边界（诚实声明）

映射层能透明隐藏：queue handle 是否相同、mutex 是否存在、submit/present
API 形状。**不能**隐藏：真实 GPU 并行吞吐；依赖两条 queue 独立前进的调度
算法；任意 wait-before-signal 图在单 queue 上的前进性。

SubmissionToken（§5.1）保证 005 式链路（submit → token → 下一个 submit）
在折叠场景下的前进性。若未来需要任意 future-timeline wait，需要真正的
submission scheduler——那不是 mutex 能解决的问题，明确留作后续阶段。

---

## 9. 已否决的替代方案

| 方案 | 否决理由 |
| --- | --- |
| 所有 queue 始终 mutex | 单线程引擎/独立 queue 路径付无谓开销；且 tag 模型下"何时需要锁"已可静态精确判定 |
| 第二次 acquire 时 emplace mutex（v1） | 运行期时序耦合，需要编译器无法检查的冻结纪律；"mapping ≠ copy"双语义；tag 已知信息被推迟重建 |
| 内建 Graphics/Compute/Transfer/Present 角色枚举 | 替用户做决策；角色数量与语义应属上层 |
| `shared_ptr<mutex>` 于 LogicalQueue | 所有权与 queue identity 分离、refcount 开销 |
| 虚基类 / `variant<Direct,Locked>` | dispatch 开销与复杂度均无必要 |
| copy LogicalQueue 时动态启锁 | 已发生的无锁 access 无法补锁；copy 带隐式副作用 |
| 启发式分配（贪心不回溯） | 规模太小，精确搜索免费；启发式结果难推理、难测试 |
| 运行期动态新增 request | 违反 Vulkan device 创建后不可扩展的硬约束 |

---

## 10. 验证计划

### 10.1 类型语义

```cpp
static_assert(std::copy_constructible<LogicalQueue>);
static_assert(not std::copy_constructible<QueueAccess>);
static_assert(std::movable<QueueAccess>);
static_assert(not std::movable<DeviceQueue>);
static_assert(not std::movable<Queue>);
static_assert(std::is_trivially_copyable_v<QueueKey>);
```

### 10.2 分配算法单元测试（纯函数，无 Vulkan 依赖）

以合成 family 拓扑驱动 planner，断言 QueuePlan：

- §6.4 用例 A–F 逐一覆盖（含本机 6-family 拓扑与单 queue 极端设备）；
- 同 tag 折叠不产生 mutex、不消耗名额；
- 跨 tag 有名额 → 拆分；无名额 → 恰好一个 Shared；
- 同 tag 异质 signature 在名额富余时自动拆分（原则 3），紧张时让位于跨 tag 拆分；
- 词典序：构造 C1/C2 冲突的拓扑，确认宁可牺牲 dedicated 也不引入 mutex；
- 不可满足 request → 指明 QueueKey 的 error；
- 确定性：同输入两次运行结果逐字节一致。

### 10.3 并发与集成

- Shared DeviceQueue：两线程并发 `acquireAccess()` 串行化（TSAN）；
- Exclusive + debug owner 断言：错误 tag 声明触发确定性 assert；
- 005 在 `queueCount==1` 设备（或以 device 层强制模拟）频繁 resize：
  无 validation error；
- `queueCount>=2`：确认 present 独立 index 且全程无 mutex（日志断言）；
- validation layers 下反复 resize/最小化/恢复；
- 销毁顺序：Swapchain → DeviceContext → InstanceContext 无对象泄漏/悬垂。

---

## 11. 迁移路径（自当前代码）

1. 新增 `queue/Queue.h` 定稿类型（修复现原型的完整性/顺序/move 问题）
   + `queue/QueuePlanner.{h,cpp}`（纯函数，先行单测）。
2. `DeviceContext` 取代 `RenderDeviceContext` 的设备所有权；
   `DeviceContextCreateInfo` 增加 `addQueueRequest`。
3. `QueueContext` 迁移至 `queue/Queue.h` 并更名 `Queue`；`create(vk::Device, LogicalQueue)`；删除公开 `getQueue()`；
   `submit()` 返回 SubmissionToken。
4. Swapchain 改为 `create(device, allocator, logical_queue, std::move(unique_surface), ...)`，
   内部自建 present Queue，
   删除裸 queue 成员。
5. 005 迁移：三个 request（§3.2 形态），删除 `getQueue(family, 1)` 及
   f24460b 的"每 family 请求 2 条 queue"临时方案。



```C++

// 002_hello_context: solve_queue_requests 正确性测试（单文件版）
//
// 按 vkc-queue-abstraction-design-v3.md §6 在本文件内定义算法与数据结构，
// queue family 拓扑由测试自行构造（不依赖真实 VkPhysicalDevice）。
// 生产版签名为 solve_queue_requests(vk::PhysicalDevice, span<const QueueRequest>)，
// 本文件直接以 span<const FamilyInfo> 作为测试入口，并额外返回 Cost 供断言。

#include <vulkan/vulkan.hpp>
#include <algorithm>
#include <cstdint>
#include <expected>
#include <numeric>
#include <optional>
#include <span>
#include <system_error>
#include <tuple>
#include <unordered_map>
#include <utility>
#include <vector>
#include <ranges>
#include "log.h"

namespace stdr = std::ranges;
namespace stdv = std::views;

namespace lcf::vkc {

// ===== 公开类型（§3.1 / §6.1） =====

enum class QueueSubmissionThreadTag : uint8_t {};
// 提交线程域。同 tag = 用户承诺不并发调用（同一线程，或已被上层串行化）。

struct QueueRequest
{
    vk::QueueFlags required_flags = {};        // 硬约束：family flags 须为其超集
    vk::QueueFlags undesired_flags = {};       // 软偏好：尽量避开含这些位的 family
    vk::SurfaceKHR present_surface = nullptr;  // 硬约束：须 getSurfaceSupportKHR == true
    QueueSubmissionThreadTag submission_thread_tag = {};
    float priority = 1.0f;
};

enum class SharingMode : uint8_t { Exclusive, Shared };  // 生产版为 DeviceQueue::SharingMode

struct DeviceQueueInfo
{
    uint32_t family_index = 0;
    uint32_t queue_index = 0;
    std::vector<uint32_t> request_indices;   // 映射到本条 queue 的 requests（声明序）
    SharingMode sharing_mode = SharingMode::Exclusive;
    float queue_priority = 1.0f;             // pQueuePriorities = max(成员 priority)
};

namespace details {

struct FamilyInfo
{
    vk::QueueFlags effective_flags = {};  // graphics/compute family 隐含 transfer（规范）
    uint32_t queue_count = 0;
    std::vector<uint8_t> present_support;    // 按 request 下标；无 surface 的 request 恒 true
};

// 某 request 的候选 family 列表（按 family index 升序）。undesired 偏好完全由
// C2 承担，候选无需排序：穷举求值下顺序不影响结果，也不影响并列裁决——
// best-fit 与 fit 的两个方案 C2 必不同，不构成并列。若日后引入分支限界，
// 再恢复 best-fit 在前的排列以获得"首叶即贪心解"的剪枝起点。
using CandidateFamilies = std::vector<uint32_t>;

// 词典序代价：operator<=> 按成员声明序比较，越小越好。
// C1/C3 统计"折叠次数"而非 Shared queue 条数——与折叠的分布方式无关。
struct Cost
{
    uint32_t forced_share_count = 0;    // C1: Σ_queue (驻留 tag 数 - 1) → mutex
    uint32_t fit_fallback_count = 0;    // C2: 落在含 undesired 位 family 的 request 数
    uint32_t mixed_signature_count = 0; // C3: Σ_queue (signature 数 - tag 数)
    uint32_t total_queue_count = 0;     // C4: VkQueue 总数
    auto operator<=>(const Cost &) const = default;
    Cost & operator+=(const Cost & other) noexcept   // 跨 family 累加
    {
        forced_share_count += other.forced_share_count;
        fit_fallback_count += other.fit_fallback_count;
        mixed_signature_count += other.mixed_signature_count;
        total_queue_count += other.total_queue_count;
        return *this;
    }
};

struct EvaluatedPlan
{
    std::vector<DeviceQueueInfo> device_queue_infos;
    Cost cost;
};

// QueueSignature：能力聚类的判据（tag 不在其中——tag 是外层分组维度，见
// assign_queue_indices 的两级结构）。刻意不含 present_surface——surface 的
// 职责止步于候选过滤：present 不占 GPU 执行，拆开不同 surface 的 present
// 不增加并行度，合并反而允许一次 presentKHR 携带多个 swapchain。也不含
// priority——同 signature 不同 priority 仍应同队列（priority 取 max）。
// 因此这里的 operator== 是真正的全成员相等，可以 default。
struct QueueSignature
{
    vk::QueueFlags required_flags = {};
    vk::QueueFlags undesired_flags = {};
    bool operator==(const QueueSignature &) const noexcept = default;
};

inline QueueSignature signature_of(const QueueRequest & request) noexcept
{
    return {request.required_flags, request.undesired_flags};
}

} // namespace details
} // namespace lcf::vkc

template <>
struct std::hash<lcf::vkc::details::QueueSignature>
{
    uint64_t operator()(const lcf::vkc::details::QueueSignature & signature) const noexcept
    {
        return static_cast<uint64_t>(static_cast<uint32_t>(signature.required_flags)) << 32 |
            static_cast<uint64_t>(static_cast<uint32_t>(signature.undesired_flags));
    }
};

namespace lcf::vkc {
namespace details {

// ===== ② 候选 family：纯硬过滤（required + surface）=====
std::vector<CandidateFamilies> compute_candidate_sets(std::span<const FamilyInfo> families, std::span<const QueueRequest> requests) noexcept
{
    std::vector<CandidateFamilies> candidates(requests.size());
    for (uint32_t request_id = 0; request_id < requests.size(); ++request_id) {
        const QueueRequest & request = requests[request_id];
        auto & candidate = candidates[request_id];
        for (const auto & [family_index, family_info] : families | stdv::enumerate) {
            bool requirement_satisfied = (family_info.effective_flags & request.required_flags) == request.required_flags;
            if (requirement_satisfied and family_info.present_support[request_id]) { candidate.push_back(family_index); }
        }
        if (candidate.empty()) { return {}; }
    }
    return candidates;
}

// ===== ④ 内层闭式计算：单 family 内的 queue index 分配 =====
// 返回本 family 的 DeviceQueueInfo 与 C1/C3/C4（C2 归 evaluate 层，恒为 0）
inline EvaluatedPlan assign_queue_indices(
    uint32_t family_index,
    std::span<const uint32_t> request_ids,
    std::span<const QueueRequest> requests,
    uint32_t required_queue_count) noexcept
{
    // signature 子组是原子单元：完全相同的 request 永远同队列（原则 4）。
    // tag 组是正确性边界：跨 tag 折叠才需要 mutex（原则 1）。
    using SignatureGroup = std::vector<uint32_t>;  // 同 signature 的 request ids（声明序）
    using SignatureCluster = std::unordered_map<QueueSignature, SignatureGroup>;
    struct TagGroup
    {
        void mergeRequest(uint32_t id, const QueueRequest & request) noexcept
        {
            m_signature_cluster[signature_of(request)].emplace_back(id);
            m_max_priority = std::max(m_max_priority, request.priority);
            ++m_member_count;
        }
        // 自然序：优先级高者为大，并列时成员多者为大（排序方向由调用点决定）
        bool operator>(const TagGroup & other) const noexcept
        {
            if (m_max_priority != other.m_max_priority) { return m_max_priority > other.m_max_priority; }
            return m_member_count > other.m_member_count;
        }

        SignatureCluster m_signature_cluster;
        float m_max_priority = 0.0f;
        uint32_t m_member_count = 0;
    };
    // WorkingQueue 的子组必须是扁平 vector 而非 SignatureCluster：溢出合并时
    // 异 tag 同能力的子组会在 map 中按 key 碰撞而被错误合并（signature 不含 tag）
    struct WorkingQueue
    {
        WorkingQueue() = default;
        explicit WorkingQueue(TagGroup && tag_group) noexcept :
            m_sub_groups(tag_group.m_signature_cluster | stdv::values | stdv::as_rvalue | stdr::to<std::vector>()),
            m_max_priority(tag_group.m_max_priority) {}
        // 溢出并入：跨 tag 合并，此后该 queue 需要 mutex
        void absorb(TagGroup && tag_group) noexcept
        {
            m_sub_groups.append_range(tag_group.m_signature_cluster | stdv::values | stdv::as_rvalue);
            m_max_priority = std::max(m_max_priority, tag_group.m_max_priority);
            ++m_tag_count;
        }
        bool hasMixedSignatures() const noexcept { return m_sub_groups.size() > 1; }
        WorkingQueue splitOneSubGroup() noexcept
        {
            WorkingQueue split_queue;
            split_queue.m_sub_groups.emplace_back(std::move(m_sub_groups.back()));
            m_sub_groups.pop_back();
            return split_queue;
        }
        SharingMode getSharingMode() const noexcept { return m_tag_count > 1 ? SharingMode::Shared : SharingMode::Exclusive; }

        std::vector<SignatureGroup> m_sub_groups;
        uint32_t m_tag_count = 1;
        float m_max_priority = 0.0f;
    };

    // ---- 两级分组（单趟）：外层 tag（正确性边界）→ 内层能力 signature（聚类粒度）。
    //      组直接存进最终要排序的 vector，map 只做 tag → index 索引 ----
    std::vector<TagGroup> tag_groups;
    std::unordered_map<QueueSubmissionThreadTag, uint32_t> tag_group_id_map;
    for (uint32_t id : request_ids) {
        const QueueRequest & request = requests[id];
        auto [it, inserted] = tag_group_id_map.try_emplace( request.submission_thread_tag, static_cast<uint32_t>(tag_groups.size()));
        if (inserted) { tag_groups.emplace_back(); }
        tag_groups[it->second].mergeRequest(id, request);
    }
    // 降序：高优先级组先拿独立 index。并列组间的相对顺序无语义，不再细分。
    // std::greater 而非 ranges::greater：后者要求 totally_ordered（全套比较符）
    stdr::sort(tag_groups, std::greater{});

    std::vector<WorkingQueue> queues;

    // ---- ① 每个 tag 组占一个独立 index；溢出组并入 max_priority 最小的 queue ----
    // 并入目标选 max_priority 最小者：让高优先级 queue 尽量保持无锁独占。
    // 不变量：溢出与空余 index 不会同时存在（溢出 ⟹ queues.size()==queue_count）。
    for (TagGroup & tg : tag_groups) {
        if (queues.size() < required_queue_count) {
            queues.emplace_back(std::move(tg));
        } else {
            auto & q = *std::ranges::min_element(queues, {}, &WorkingQueue::m_max_priority);
            q.absorb(std::move(tg));
        }
    }

    // ---- ② 空余 index 用于分散：不同 signature 尽量不同队列（原则 3） ----
    // ① 的不变量保证此处所有 queue 都是单 tag（Exclusive），拆分不影响 C1。
    // 同 tag 分布在多条 queue 不需要锁——tag 约束"不并发调用"，与 queue 条数无关。
    while (queues.size() < required_queue_count) {
        // 拆分的受益者：最拥挤（signature 子组最多）的 queue
        auto beneficiary = stdr::max_element(queues, {}, [](const WorkingQueue & q) { return q.m_sub_groups.size(); });
        if (not beneficiary->hasMixedSignatures()) { break; }
        queues.emplace_back(beneficiary->splitOneSubGroup());
    }

    // ---- ③ 汇总：C1/C3/C4 由本层的决策直接得出（tag_count / sub_groups 数 / queue 数），
    //      不留给上层从扁平输出中重新考古。C2 是 family 落点的代价，归 evaluate 层
    EvaluatedPlan plan;
    plan.device_queue_infos.reserve(queues.size());
    plan.cost.total_queue_count = static_cast<uint32_t>(queues.size());          // C4
    for (uint32_t index = 0; index < queues.size(); ++index) {
        auto & queue = queues[index];
        plan.cost.forced_share_count += queue.m_tag_count - 1;                   // C1
        plan.cost.mixed_signature_count += static_cast<uint32_t>(queue.m_sub_groups.size()) - queue.m_tag_count; // C3
        DeviceQueueInfo info{.family_index = family_index, .queue_index = index};
        info.request_indices = queue.m_sub_groups | stdv::join | stdr::to<std::vector>();
        info.sharing_mode = queue.getSharingMode();
        info.queue_priority = stdr::max(info.request_indices | stdv::transform([&](uint32_t id) { return requests[id].priority; }));
        plan.device_queue_infos.emplace_back(std::move(info));
    }
    return plan;
}

// ===== ⑤ 叶子求值：完整 request→family 映射 → 逐 family 调 ④，汇总 Cost =====
inline EvaluatedPlan evaluate_assignment(
    std::span<const uint32_t> family_of_request,
    std::span<const QueueRequest> requests,
    std::span<const FamilyInfo> families) noexcept
{
    EvaluatedPlan plan;
    std::vector<std::vector<uint32_t>> per_family(families.size());
    for (uint32_t i = 0; i < requests.size(); ++i) {
        per_family[family_of_request[i]].push_back(i);        // i 递增 → 桶内声明序
    }
    for (uint32_t f = 0; f < families.size(); ++f) {
        if (per_family[f].empty()) { continue; }
        auto family_plan = assign_queue_indices(f, per_family[f], requests, families[f].queue_count);
        plan.cost += family_plan.cost;
        plan.device_queue_infos.append_range(std::move(family_plan.device_queue_infos));
    }
    plan.cost.fit_fallback_count = static_cast<uint32_t>(stdr::count_if(stdv::zip(requests, family_of_request),
    [&](const auto & zipped) {
        const auto & [request, family_index] = zipped;
        return bool(families[family_index].effective_flags & request.undesired_flags);
    }));
    return plan;
}

// ===== ⑥ DFS 驱动（生产版 solve_queue_requests 的核心；测试入口见下） =====
struct SearchContext
{
    std::span<const QueueRequest> requests;
    std::span<const FamilyInfo> families;
    std::span<const CandidateFamilies> candidates;   // 仅驱动 DFS 枚举顺序
    std::span<const uint32_t> order;
    std::vector<uint32_t> family_of_request;
    std::optional<EvaluatedPlan> best;
};

inline void dfs(SearchContext & ctx, size_t depth) noexcept
{
    if (depth == ctx.order.size()) {
        auto plan = evaluate_assignment(ctx.family_of_request, ctx.requests, ctx.families);
        if (not ctx.best or plan.cost < ctx.best->cost) { ctx.best = std::move(plan); }
        return;
    }
    uint32_t request_index = ctx.order[depth];
    for (uint32_t f : ctx.candidates[request_index]) {
        ctx.family_of_request[request_index] = f;
        dfs(ctx, depth + 1);
    }
}

} // namespace details

// 测试入口：families 直接注入（生产版由 collect_family_infos(physical_device) 提供），
// 并返回含 Cost 的 EvaluatedPlan 以便断言。
inline std::expected<details::EvaluatedPlan, std::error_code>
solve_queue_requests(std::span<const details::FamilyInfo> families, std::span<const QueueRequest> requests) noexcept
{
    using namespace details;
    auto candidates = compute_candidate_sets(families, requests);
    if (candidates.empty()) { return std::unexpected(std::make_error_code(std::errc::invalid_argument)); }
    auto order = stdv::iota(0u, static_cast<uint32_t>(requests.size())) | stdr::to<std::vector>();
    stdr::sort(order, [&](uint32_t a, uint32_t b) {
        if (candidates[a].size() != candidates[b].size()) { return candidates[a].size() < candidates[b].size(); }
        if (requests[a].priority != requests[b].priority) { return requests[a].priority > requests[b].priority; }
        return a < b;
    }); //- search order, the more constraint, the higher priority
    SearchContext ctx {
        requests,
        families,
        std::move(candidates),
        std::move(order),
        std::vector<uint32_t>(requests.size()),
        std::nullopt
    };
    dfs(ctx, 0);
    return std::move(*ctx.best);
}

} // namespace lcf::vkc

// ======================= 测试 =======================

using namespace lcf;
using namespace lcf::vkc;
using details::FamilyInfo, details::EvaluatedPlan, details::Cost;

namespace {

int g_failures = 0;

#define CHECK(cond)                                                             \
    do {                                                                        \
        if (!(cond)) {                                                          \
            ++g_failures;                                                       \
            lcf_log_error("CHECK failed: {} (line {})", #cond, __LINE__);       \
        }                                                                       \
    } while (0)

constexpr auto G  = vk::QueueFlagBits::eGraphics;
constexpr auto C  = vk::QueueFlagBits::eCompute;
constexpr auto T  = vk::QueueFlagBits::eTransfer;
constexpr auto SB = vk::QueueFlagBits::eSparseBinding;

// 假 surface handle：present_support 由测试预先注入，绝不会传给 Vulkan
const vk::SurfaceKHR fake_surface(reinterpret_cast<VkSurfaceKHR>(uintptr_t{0x1}));

// 构造 FamilyInfo：折算 effective flags，按 can_present 填 present_support
FamilyInfo make_family(vk::QueueFlags flags, uint32_t queue_count, bool can_present,
                       std::span<const QueueRequest> requests)
{
    FamilyInfo info;
    if (flags & (G | C)) { flags |= T; }   // 规范：graphics/compute 隐含 transfer
    info.effective_flags = flags;
    info.queue_count = queue_count;
    info.present_support = requests | stdv::transform([&](const QueueRequest & request) {
        return static_cast<uint8_t>(!request.present_surface || can_present);
    }) | stdr::to<std::vector>();
    return info;
}

// 返回包含 request i 的 DeviceQueueInfo（必须恰好一个）
const DeviceQueueInfo & queue_of(const EvaluatedPlan & plan, uint32_t request)
{
    const DeviceQueueInfo * found = nullptr;
    for (const auto & q : plan.device_queue_infos) {
        if (std::ranges::contains(q.request_indices, request)) {
            CHECK(found == nullptr);   // 每个 request 恰好映射一次
            found = &q;
        }
    }
    CHECK(found != nullptr);
    static const DeviceQueueInfo dummy{};
    return found ? *found : dummy;
}

void log_plan(const char * name, const EvaluatedPlan & plan)
{
    lcf_log_info("---- {} ----", name);
    for (const auto & q : plan.device_queue_infos) {
        std::string members;
        for (uint32_t id : q.request_indices) { members += std::format("R{} ", id); }
        lcf_log_info("  family {}, queue {}, {}, priority {:.2f}, requests: {}",
            q.family_index, q.queue_index,
            q.sharing_mode == SharingMode::Shared ? "Shared   " : "Exclusive",
            q.queue_priority, members);
    }
    lcf_log_info("  cost = {{C1={}, C2={}, C3={}, C4={}}}",
        plan.cost.forced_share_count, plan.cost.fit_fallback_count,
        plan.cost.mixed_signature_count, plan.cost.total_queue_count);
}

// 标准 request 集（用例 A–D）：tag 全部互异，模拟多线程引擎
// R0 主渲染   R1 present(blit)   R2 异步计算   R3 上传
std::vector<QueueRequest> standard_requests()
{
    return {
        {.required_flags = G, .submission_thread_tag = QueueSubmissionThreadTag{0}, .priority = 1.0f},
        {.required_flags = G, .present_surface = fake_surface,
         .submission_thread_tag = QueueSubmissionThreadTag{1}, .priority = 0.9f},
        {.required_flags = C, .undesired_flags = G,
         .submission_thread_tag = QueueSubmissionThreadTag{2}, .priority = 0.8f},
        {.required_flags = T, .undesired_flags = G | C,
         .submission_thread_tag = QueueSubmissionThreadTag{3}, .priority = 0.5f},
    };
}

// 用例 A：真实桌面 GPU 拓扑（本机）——四个互异 request 全部独立、零锁
void test_a_desktop_gpu()
{
    auto requests = standard_requests();
    std::vector<FamilyInfo> families = {
        make_family(G | C | T | SB, 16, true,  requests),  // family 0
        make_family(T | SB,          2, false, requests),  // family 1
        make_family(C | T | SB,      8, false, requests),  // family 2
        make_family(T | SB,          2, false, requests),  // family 3 (video decode)
        make_family(T | SB,          2, false, requests),  // family 4 (video encode)
        make_family(T | SB,          1, false, requests),  // family 5 (optical flow)
    };
    auto plan = solve_queue_requests(families, requests);
    CHECK(plan.has_value());
    log_plan("A: desktop GPU", *plan);

    CHECK((plan->cost == Cost{0, 0, 0, 4}));
    const auto & q0 = queue_of(*plan, 0);   // gfx -> (0, x)
    const auto & q1 = queue_of(*plan, 1);   // present -> (0, y), y != x
    const auto & q2 = queue_of(*plan, 2);   // compute -> 专用 compute family 2
    const auto & q3 = queue_of(*plan, 3);   // transfer -> 专用 transfer family 1
    CHECK(q0.family_index == 0 && q1.family_index == 0);
    CHECK(q0.queue_index != q1.queue_index);
    CHECK(q2.family_index == 2);
    CHECK(q3.family_index == 1);
    for (const auto & q : plan->device_queue_infos) {
        CHECK(q.sharing_mode == SharingMode::Exclusive);
    }
}

// 用例 B：相同 signature 聚合——R4 复制 R0，即使空 queue 充裕也同队列
void test_b_identical_requests_merge()
{
    auto requests = standard_requests();
    requests.push_back({.required_flags = G,
                        .submission_thread_tag = QueueSubmissionThreadTag{0},
                        .priority = 0.7f});   // R4 = R0 的 signature，priority 不同
    std::vector<FamilyInfo> families = {
        make_family(G | C | T | SB, 16, true,  requests),
        make_family(T | SB,          2, false, requests),
        make_family(C | T | SB,      8, false, requests),
    };
    auto plan = solve_queue_requests(families, requests);
    CHECK(plan.has_value());
    log_plan("B: identical requests merge", *plan);

    const auto & q0 = queue_of(*plan, 0);
    const auto & q4 = queue_of(*plan, 4);
    CHECK(q0.family_index == q4.family_index && q0.queue_index == q4.queue_index);
    CHECK(q0.sharing_mode == SharingMode::Exclusive);   // 同 tag，无锁
    CHECK(q0.queue_priority == 1.0f);                   // max(1.0, 0.7)
    CHECK(plan->cost.mixed_signature_count == 0);       // 相同 signature 不算异质折叠
}

// 用例 C：极端单队列设备——一个 mutex 保护一切
void test_c_single_queue_device()
{
    auto requests = standard_requests();
    std::vector<FamilyInfo> families = {
        make_family(G | C | T, 1, true, requests),
    };
    auto plan = solve_queue_requests(families, requests);
    CHECK(plan.has_value());
    log_plan("C: single-queue device", *plan);

    CHECK((plan->cost == Cost{3, 2, 0, 1}));            // C2: R2/R3 落在含 undesired 的 fam0
    CHECK(plan->device_queue_infos.size() == 1);
    CHECK(plan->device_queue_infos[0].sharing_mode == SharingMode::Shared);
    CHECK(plan->device_queue_infos[0].request_indices.size() == 4);
    CHECK(plan->device_queue_infos[0].queue_priority == 1.0f);
}

// 用例 D：单 family 双 queue——高优先级 queue 保持无锁独占，争用集中到低优先级
void test_d_two_queues_protect_high_priority()
{
    auto requests = standard_requests();
    std::vector<FamilyInfo> families = {
        make_family(G | C | T, 2, true, requests),
    };
    auto plan = solve_queue_requests(families, requests);
    CHECK(plan.has_value());
    log_plan("D: two queues", *plan);

    CHECK((plan->cost == Cost{2, 2, 0, 2}));
    const auto & q_gfx = queue_of(*plan, 0);
    CHECK(q_gfx.request_indices.size() == 1);           // 主渲染独占
    CHECK(q_gfx.sharing_mode == SharingMode::Exclusive);
    const auto & q_rest = queue_of(*plan, 1);
    CHECK(q_rest.request_indices.size() == 3);          // R1/R2/R3 共享
    CHECK(q_rest.sharing_mode == SharingMode::Shared);
    CHECK(q_rest.queue_priority == 0.9f);
}

// 用例 E：同 tag 异质的分散与回退（原则 3 + §2 判定式）
void test_e_same_tag_spread_and_fold()
{
    std::vector<QueueRequest> requests = {
        {.required_flags = G, .submission_thread_tag = QueueSubmissionThreadTag{0}, .priority = 1.0f},
        {.required_flags = C, .submission_thread_tag = QueueSubmissionThreadTag{0}, .priority = 0.8f},
        {.required_flags = G, .present_surface = fake_surface,
         .submission_thread_tag = QueueSubmissionThreadTag{1}, .priority = 0.9f},
    };
    for (uint32_t queue_count : {3u, 2u, 1u}) {
        std::vector<FamilyInfo> families = {
            make_family(G | C | T, queue_count, true, requests),
        };
        auto plan = solve_queue_requests(families, requests);
        CHECK(plan.has_value());
        log_plan(std::format("E: same-tag hetero, queue_count={}", queue_count).c_str(), *plan);

        const auto & q_gfx  = queue_of(*plan, 0);
        const auto & q_comp = queue_of(*plan, 1);
        const auto & q_pres = queue_of(*plan, 2);
        switch (queue_count) {
        case 3:   // 名额富余：三者全部分散，全 Exclusive
            CHECK(q_gfx.queue_index != q_comp.queue_index);
            CHECK(q_gfx.queue_index != q_pres.queue_index);
            CHECK(plan->cost.forced_share_count == 0 && plan->cost.mixed_signature_count == 0);
            break;
        case 2:   // 同线程折叠优于加锁：gfx+comp 同队列且 Exclusive
            CHECK(q_gfx.queue_index == q_comp.queue_index);
            CHECK(q_gfx.sharing_mode == SharingMode::Exclusive);
            CHECK(q_pres.queue_index != q_gfx.queue_index);
            CHECK((plan->cost == Cost{0, 0, 1, 2}));
            break;
        case 1:   // 全折叠：跨 tag → Shared
            CHECK(q_gfx.queue_index == q_pres.queue_index);
            CHECK(q_gfx.sharing_mode == SharingMode::Shared);
            CHECK((plan->cost == Cost{1, 0, 1, 1}));
            break;
        }
    }
}

// 用例 F：回溯优于贪心——xfer 跟随同 tag 的 comp 落 family 1，消掉 mutex
void test_f_backtracking_beats_greedy()
{
    std::vector<QueueRequest> requests = {
        {.required_flags = G, .submission_thread_tag = QueueSubmissionThreadTag{0}, .priority = 1.0f},
        {.required_flags = C, .submission_thread_tag = QueueSubmissionThreadTag{1}, .priority = 0.9f},
        {.required_flags = T, .submission_thread_tag = QueueSubmissionThreadTag{1}, .priority = 0.5f},
    };
    std::vector<FamilyInfo> families = {
        make_family(G | T, 1, true,  requests),   // family 0：贪心会把 xfer 放这
        make_family(C | T, 1, false, requests),   // family 1：最优解在这（与 comp 同 tag 折叠）
    };
    auto plan = solve_queue_requests(families, requests);
    CHECK(plan.has_value());
    log_plan("F: backtracking beats greedy", *plan);

    CHECK(plan->cost.forced_share_count == 0);          // 贪心首叶是 C1=1
    const auto & q_comp = queue_of(*plan, 1);
    const auto & q_xfer = queue_of(*plan, 2);
    CHECK(q_xfer.family_index == 1);
    CHECK(q_comp.queue_index == q_xfer.queue_index);
    CHECK(q_xfer.sharing_mode == SharingMode::Exclusive);
    CHECK((plan->cost == Cost{0, 0, 1, 2}));
}

// 用例 G：不可满足的 request 显式报错
void test_g_unsatisfiable()
{
    std::vector<QueueRequest> requests = {
        {.required_flags = G, .present_surface = fake_surface,
         .submission_thread_tag = QueueSubmissionThreadTag{0}},
    };
    std::vector<FamilyInfo> families = {
        make_family(G | T, 1, false, requests),   // 有 graphics 但不支持 present
    };
    auto plan = solve_queue_requests(families, requests);
    CHECK(!plan.has_value());
}

// 用例 H：确定性——同输入两次求解结果逐字段一致
void test_h_determinism()
{
    auto requests = standard_requests();
    std::vector<FamilyInfo> families = {
        make_family(G | C | T | SB, 2, true,  requests),
        make_family(C | T | SB,     1, false, requests),
        make_family(T | SB,         1, false, requests),
    };
    auto p1 = solve_queue_requests(families, requests);
    auto p2 = solve_queue_requests(families, requests);
    CHECK(p1.has_value() && p2.has_value());
    log_plan("H: determinism", *p1);
    CHECK(p1->cost == p2->cost);
    CHECK(p1->device_queue_infos.size() == p2->device_queue_infos.size());
    for (size_t i = 0; i < p1->device_queue_infos.size(); ++i) {
        const auto & a = p1->device_queue_infos[i];
        const auto & b = p2->device_queue_infos[i];
        CHECK(a.family_index == b.family_index && a.queue_index == b.queue_index);
        CHECK(a.request_indices == b.request_indices);
        CHECK(a.sharing_mode == b.sharing_mode);
    }
}

// 用例 I：多窗口 present 合并——surface 不参与 signature：同 tag 同 flags、
// 仅 surface 不同的两个 present 是"相同 request"，即使名额富余也同队列
// （present 不占 GPU 执行，合并允许一次 presentKHR 携带多个 swapchain）
void test_i_multi_window_presents_merge()
{
    const vk::SurfaceKHR another_surface(reinterpret_cast<VkSurfaceKHR>(uintptr_t{0x2}));
    std::vector<QueueRequest> requests = {
        {.required_flags = G, .submission_thread_tag = QueueSubmissionThreadTag{0}, .priority = 1.0f},
        {.required_flags = G, .present_surface = fake_surface,
         .submission_thread_tag = QueueSubmissionThreadTag{1}, .priority = 0.9f},
        {.required_flags = G, .present_surface = another_surface,
         .submission_thread_tag = QueueSubmissionThreadTag{1}, .priority = 0.9f},
    };
    std::vector<FamilyInfo> families = {
        make_family(G | C | T, 8, true, requests),   // 名额远超需求
    };
    auto plan = solve_queue_requests(families, requests);
    CHECK(plan.has_value());
    log_plan("I: multi-window presents merge", *plan);

    const auto & q_p0 = queue_of(*plan, 1);
    const auto & q_p1 = queue_of(*plan, 2);
    CHECK(q_p0.queue_index == q_p1.queue_index);          // 两个 present 同队列
    CHECK(q_p0.sharing_mode == SharingMode::Exclusive);   // 同 tag，无锁
    CHECK(q_p0.queue_index != queue_of(*plan, 0).queue_index);
    CHECK((plan->cost == Cost{0, 0, 0, 2}));              // 相同 request 不算 C3 异质折叠
}

} // namespace

int main()
{
    log::init();
    test_a_desktop_gpu();
    test_b_identical_requests_merge();
    test_c_single_queue_device();
    test_d_two_queues_protect_high_priority();
    test_e_same_tag_spread_and_fold();
    test_f_backtracking_beats_greedy();
    test_g_unsatisfiable();
    test_h_determinism();
    test_i_multi_window_presents_merge();

    if (g_failures == 0) {
        lcf_log_info("all solve_queue_requests tests passed");
        return 0;
    }
    lcf_log_error("{} check(s) failed", g_failures);
    return 1;
}
```


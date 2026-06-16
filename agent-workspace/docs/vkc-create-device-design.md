# vkc `create_device` 实现设计

设计 `libs/vk_core/include/vk_core/bootstrap/create_device.h` 的实现，重点回答两个问题：
**(1) `DeviceCreateInfo` 应该携带什么；(2) `create_device` 该不该返回 queue 信息。**
一次性文档，看完即弃；代码是骨架，错误处理与边界你按 `create_instance.cpp` 的
`_maythrow` / `noexcept` 模式补全。给文档不改代码。

参考锚点：
- `libs/vk_core/src/bootstrap/create_instance.cpp` —— `_maythrow` 私有实现 + `noexcept`
  边界 catch `vk::SystemError` 的既定模式，本文严格照搬。
- `libs/vk_core/src/bootstrap/select_physical_device.cpp` —— 已实现的同级原语，
  `DeviceCreateInfo` 的 setter / manifest 指针风格照它和 `PhysicalDeviceSelectInfo` 走。
- `libs/vk_core/include/vk_core/manifest/DeviceExtensionManifest.h` —— 扩展 + 特性的载体，
  `getRequiredFeatures()` 已返回串好 pNext 的 `vk::PhysicalDeviceFeatures2 &`，本文直接复用。
- `agent-workspace/docs/vkc-creation-layering.md` §4 —— 「create_device 纯机械、不认识
  QueueRole」的分层结论，本文是它的落地。
- `agent-workspace/docs/vkc-device-queue-creation-guide.md` §7 —— 上层 `DeviceContext::create`
  如何算出 `{family, count}` 并在 device 建好后取 queue。本原语是它的底座。

---

## 0. 定位：和 create_instance / select 同级的 bootstrap 原语

`create_device(physical_device, info)` 干一件事：把调用方**已经算好的**机械参数
（要启用的扩展 + 特性 + 每个族建几个队列）组装成 `vk::DeviceCreateInfo`，`createDeviceUnique`
出一个 `vk::UniqueDevice`，并 `initialize_device` 加载 device 级函数指针。

它**不**做这些（都是上层 Context / DeviceContext 的职责）：

- 不认识 `DeviceRole` / `QueueRole`，不跑 collapse ladder、不算「理想队列数」。
- 不做按角色去重、不合并多角色的 manifest 并集。
- 不碰 surface / present —— queue 描述只有 `{family_index, queue_count}`，从签名上就杜绝。

> 判定准绳（layering 文档）：**不需要知道任何 vkc 发明的枚举就能跑完的逻辑，才属于这一层。**
> queue 拓扑是 vkc 的 GPU 模型，留在 Context；create_device 只看到「在 family 3 建 2 个、
> family 5 建 1 个」这种纯 vk 描述。

---

## 1. 问题一：`DeviceCreateInfo` 应该携带什么

`vk::DeviceCreateInfo` 本身需要四样东西，逐一对应到我们的 `DeviceCreateInfo` 字段：

| vk::DeviceCreateInfo 需要 | 来源 | DeviceCreateInfo 怎么存 |
|---|---|---|
| `pEnabledExtensionNames` | 要启用的设备扩展 | 复用 `DeviceExtensionManifest`（指针） |
| `pNext` → 特性链 | `vk::PhysicalDeviceFeatures2` + 扩展特性结构 | 复用 `DeviceExtensionManifest::getRequiredFeatures()` |
| `pQueueCreateInfos` | 每个族建几个队列 | `vector<pair<uint32_t,uint32_t>>` = `{family_index, queue_count}` |
| (layer，已废弃) | —— | 不存，device layer 是历史遗留 |

所以 `DeviceCreateInfo` 只需两个逻辑输入:**一个 manifest 指针**（扩展+特性一并带走）和
**一份 queue 描述**。manifest 复用是关键——`select_physical_device` 已经用它做过硬过滤，
build device 时同一份 manifest 直接拿来 enable，无需重新罗列扩展或重建特性链。

### 1.1 完整定义（替换 `create_infos.h:99-112` 的空壳）

沿用本文件既定的 `Self` 别名、删拷贝留移动、链式 setter 风格，与 `InstanceCreateInfo` /
`PhysicalDeviceSelectInfo` 一致。

```cpp
// create_infos.h —— 替换现有 DeviceCreateInfo 空壳
class DeviceCreateInfo
{
    using Self = DeviceCreateInfo;
    using QueueFamilyRequest = std::pair<uint32_t, uint32_t>;   // {family_index, queue_count}
    using QueueFamilyRequestList = std::vector<QueueFamilyRequest>;
public:
    ~DeviceCreateInfo() noexcept;                  // 出 .cpp（manifest 是不完整类型）
    DeviceCreateInfo() noexcept = default;
    DeviceCreateInfo(const Self &) = delete;
    DeviceCreateInfo(Self &&) noexcept = default;
    Self & operator =(const Self &) = delete;
    Self & operator =(Self &&) noexcept = default;
public:
    // 扩展 + 特性：与 select 阶段同一份 manifest。可空（nullptr → 不启用任何扩展/扩展特性）。
    Self & setRequiredDeviceExtensionManifest(const DeviceExtensionManifest & manifest) noexcept;

    // queue 描述：Context 用 QueueRole + collapse ladder 算出的 {family, count}。
    // create_device 不解释它，只照着建。
    Self & addQueueFamilyRequest(uint32_t family_index, uint32_t queue_count) noexcept
    {
        m_queue_family_requests.emplace_back(family_index, queue_count);
        return *this;
    }
    Self & setQueueFamilyRequests(convertible_range_of_c<QueueFamilyRequest> auto && requests) noexcept
    {
        m_queue_family_requests.assign_range(std::forward<decltype(requests)>(requests));
        return *this;
    }

    // —— 读侧（供 _maythrow 消费）——
    const DeviceExtensionManifest * getDeviceExtensionManifest() const noexcept { return m_manifest_p; }
    std::span<const QueueFamilyRequest> getQueueFamilyRequests() const noexcept { return m_queue_family_requests; }

private:
    const DeviceExtensionManifest * m_manifest_p = nullptr;
    QueueFamilyRequestList m_queue_family_requests;
};
```

> 与 `PhysicalDeviceSelectInfo` 对称:头里前置声明 `DeviceExtensionManifest`（`create_infos.h:12`
> 已有）+ 存指针，析构和 `setRequiredDeviceExtensionManifest` 出 `.cpp`，编译墙保持薄。
> `convertible_range_of_c` concept 已在用（`InstanceCreateInfo::addRequiredInstanceLayers`）。

### 1.2 为什么 queue 描述是 `{family, count}` 而不是 `vk::DeviceQueueCreateInfo`

两个原因:

1. **priority 数组生命周期。** `vk::DeviceQueueCreateInfo::pQueuePriorities` 是裸指针，必须活到
   `createDevice` 调用那一刻。若让调用方传 `DeviceQueueCreateInfo`，priority 数组的所有权就漏到
   调用方，容易 dangling。改成 `{family, count}`，priority 数组在 `create_device` 内部按 count
   现造（默认全 `1.0f`），生命周期自然包在函数体内。
2. **最小机械描述。** Context 算出来的本来就是「每族几个队列」，`DeviceQueueCreateInfo` 的其余
   字段（flags、pNext）当前都不用。传 pair 表达的信息恰好不多不少。

> 若将来要 per-queue priority 分级，扩成 `{family_index, vector<float> priorities}`，count 即
> `priorities.size()`。当前阶段不需要，保持 pair。

---

## 2. 问题二:create_device 该不该返回 queue 信息?

**结论:不返回 queue 对象，只返回 `vk::UniqueDevice`。** 这是你问的「比较难处理」的那一点，
分三步说清为什么。

### 2.1 难点的本质:queue 不是独立资源，取 queue 不会失败

`vk::Queue` 不是 `createXxx` 出来的——它随 device 一起创建，`device.getQueue(family, index)`
只是**取一个已经存在的句柄**，不分配、不会抛、不需要销毁（随 device 析构）。所以「返回 queue」
不是返回所有权，只是返回「我刚才建了哪些 (family, index) 槽位」这份信息。

这就引出真正的张力:**这份信息 Context 已经有了。** Context 传进来的 `queue_family_requests`
本身就是 `{family, count}`——它完全知道建了哪些槽位，建完之后自己 `getQueue` 即可，不需要
create_device 再吐一遍。

### 2.2 三个候选方案

| 方案 | 返回 | 评价 |
|---|---|---|
| A. 只返回 device | `vk::UniqueDevice` | **推荐**。queue 是 Context 用 QueueRole 模型自己取，符合分层 |
| B. 返回 device + queue 列表 | `{UniqueDevice, vector<vk::Queue>}` | queue 顺序/语义全靠约定，Context 还得反查哪个 queue 属于哪个 role，反而更绕 |
| C. 返回 device + QueueContext | `{UniqueDevice, vector<QueueContext>}` | **越层**。`QueueContext` 带 TimelineSemaphore、是 vkc GPU 模型，下沉到 bootstrap 就破了分层 |

方案 C 最该排除:`QueueContext`（`context/QueueContext.h`）持有 `TimelineSemaphore`、未来还要
做提交漏斗，它属于 `context/` 层的 GPU 模型。bootstrap 原语返回它，等于让「无 policy 的机械层」
依赖「有 policy 的模型层」，依赖方向倒了。

方案 B 看似省事，实则把「queue 数组的下标 ↔ (family, role) 的对应关系」变成隐式约定。Context
拿到一个扁平 `vector<vk::Queue>` 还得知道「第 0 个是 family 3 的第 0 个队列」才能用——这个映射
正是 collapse ladder 的输入，Context 本来就要自己建，不如直接自己 `getQueue`。

### 2.3 推荐:device-only，queue 由 DeviceContext 取

```cpp
// create_device.h —— 签名保持现状不动
std::expected<vk::UniqueDevice, std::error_code>
create_device(vk::PhysicalDevice physical_device, const DeviceCreateInfo & create_info) noexcept;
```

职责切线:

```
DeviceContext::create(pd, ...)                      ← Context 层，认识 QueueRole
  ├─ 解析 queue families + QueueRole 算 {family,count}   (guide §7.1-7.2)
  ├─ 组装 bs::DeviceCreateInfo(manifest + {family,count})
  ├─ auto device = bs::create_device(pd, info)            ← bootstrap 层，纯机械
  │     └─ createDeviceUnique + initialize_device，返回 UniqueDevice
  ├─ m_device = std::move(*device)
  ├─ 对每个 {family, count}: 循环 device.getQueue(family, i) 建 QueueContext  (guide §7.4)
  └─ 跑 collapse ladder 填 QueueRole 表
```

create_device 在「createDeviceUnique」这个动词上，DeviceContext 在「取 queue + 套 role」这个
决策上。**device handle 是两层之间唯一的传递物**——干净、无隐式约定、queue 模型完全留在 Context。

> 一句话:create_device **不**返回 queue 信息。queue 句柄随 device 而生、取用不会失败、且
> Context 已掌握 `{family, count}`——让 DeviceContext 拿到 device 后自己 `getQueue` 即可，
> 这样 TimelineSemaphore / QueueRole / collapse ladder 全留在 context 层，分层不破。

### 2.4 能力快照归属(顺带澄清)

layering 文档 §4 提过「谁拥有能力快照」。本设计下 manifest 是调用方（Context）持有并传入的，
create_device 不持有任何状态。device 建好后，**Context 把那份 manifest（或它的 enabled 视图）
存进 DeviceContext** 作为运行期契约（guide §9）。create_device 不返回快照——它本来就没造新东西，
特性链是 manifest 里现成的 `getRequiredFeatures()`。这比 layering §4 设想的「create_device 返回
FeatureChain」更省:manifest 模型下链子是 manifest 的成员，无需复制返回。

---

## 3. `create_device.cpp` 实现骨架

照搬 `create_instance.cpp` / `select_physical_device.cpp` 的结构:`noexcept` 公开函数 catch
`vk::SystemError`，核心逻辑放匿名命名空间的 `_maythrow` 里。

```cpp
#include "vk_core/bootstrap/create_device.h"
#include "vk_core/bootstrap/create_infos.h"
#include "vk_core/bootstrap/api_dispatch.h"
#include "vk_core/manifest/DeviceExtensionManifest.h"
#include "vk_core/error.h"

namespace stdr = std::ranges;
namespace stdv = std::views;

namespace {

using namespace lcf::vkc;
using namespace lcf::vkc::bs;

vk::UniqueDevice create_device_maythrow(vk::PhysicalDevice physical_device,
                                        const DeviceCreateInfo & create_info);

} // namespace

namespace lcf::vkc::bs {

std::expected<vk::UniqueDevice, std::error_code>
create_device(vk::PhysicalDevice physical_device, const DeviceCreateInfo & create_info) noexcept
{
    vk::UniqueDevice device{};
    try {
        device = create_device_maythrow(physical_device, create_info);
    } catch (const vk::SystemError & e) {
        return std::unexpected(e.code());
    }
    if (not device) { return std::unexpected(make_error_code(errc::no_suitable_physical_device)); }
    bs::initialize_device(device.get());
    return device;
}

} // namespace lcf::vkc::bs

namespace {

vk::UniqueDevice create_device_maythrow(vk::PhysicalDevice physical_device,
                                        const DeviceCreateInfo & create_info)
{
    // (a) 设备扩展 cstr 列表(manifest 可空)
    std::vector<const char *> extension_names_cstr;
    const DeviceExtensionManifest * manifest = create_info.getDeviceExtensionManifest();
    if (manifest) {
        extension_names_cstr = manifest->requiredExtensions()        // ← select 落地清单已补的访问器
            | stdv::transform(&std::string::c_str)
            | stdr::to<std::vector>();
    }

    // (b) queue create infos —— priority 数组在本函数内持有，活到 createDevice
    auto requests = create_info.getQueueFamilyRequests();
    std::vector<std::vector<float>> priorities;                      // 每族一组，持有到 create
    priorities.reserve(requests.size());
    std::vector<vk::DeviceQueueCreateInfo> queue_create_infos;
    queue_create_infos.reserve(requests.size());
    for (auto [family_index, queue_count] : requests) {
        priorities.emplace_back(queue_count, 1.0f);
        queue_create_infos.push_back(vk::DeviceQueueCreateInfo{}
            .setQueueFamilyIndex(family_index)
            .setQueuePriorities(priorities.back()));
    }

    // (c) 特性链:manifest 已串好 pNext，直接拿 root 当 pNext
    vk::DeviceCreateInfo device_create_info;
    device_create_info.setQueueCreateInfos(queue_create_infos)
        .setPEnabledExtensionNames(extension_names_cstr);
    if (manifest) { device_create_info.setPNext(&manifest->getRequiredFeatures()); }

    return physical_device.createDeviceUnique(device_create_info);
}

} // namespace
```

> ⚠️ **特性链生命周期**:`createDeviceUnique` 在调用那一刻沿 `pNext` 读特性链。这里 `pNext`
> 指向 `manifest` 内部的 `PhysicalDeviceFeatureChain`(node-based map，pNext 地址稳定，见
> `physical_device_feature_utils.h`)。只要 manifest 在 `create_device` 整个调用期间存活即可——
> 由调用方(DeviceContext)保证;create_device 只借 `const &`，不延长其生命周期。
>
> ⚠️ **不要用 `setPEnabledFeatures`**(老式 `VkPhysicalDeviceFeatures`)。特性走 `pNext` 的
> `PhysicalDeviceFeatures2` 链，二者互斥;用了 `pNext` 链就把 `pEnabledFeatures` 留空。

---

## 4. 落地清单(仅列，不在本文改码)

按依赖从底向上，每步可独立编译:

1. **`create_infos.h`** —— 替换 `:99-112` 的 `DeviceCreateInfo` 空壳为 §1.1 完整定义。
   需要 `#include <span>`、`#include <utility>`(pair);`<vector>` 经 vulkan.hpp 间接可用，
   稳妥起见显式加。`convertible_range_of_c` 来自已 include 的 `concepts/range_concept.h`。
2. **`create_infos.cpp`** —— 追加 `DeviceCreateInfo::~DeviceCreateInfo() = default` 和
   `setRequiredDeviceExtensionManifest`(与 `PhysicalDeviceSelectInfo` 对应实现同形)。
3. **`DeviceExtensionManifest`** —— 确认 `requiredExtensions()` const 访问器已存在(select 落地
   清单 §6.3 要求补的;若还没补，这里一并补):
   ```cpp
   const std::unordered_set<std::string> & requiredExtensions() const noexcept { return m_required_extensions; }
   ```
   create_device 的扩展并集要正向遍历它,而非 `isExtensionRequired(name)`。
4. **新建** `libs/vk_core/src/bootstrap/create_device.cpp`，按 §3 实现。
5. **错误码** —— 复用现有 `errc`(`error.h`)。「device 建失败」目前可挂 `no_suitable_physical_device`;
   若想区分，加一个 `device_creation_failed` 码。`vk::SystemError` 走边界转换，无需新码。
6. **`create_device.h`** —— 签名不动(§2.3 已是最终形态)。
7. **CMake** 无需改:`src/**/*.cpp` 是 GLOB(`libs/vk_core/CMakeLists.txt`),新增 .cpp 自动纳入,
   重新 configure 一次让 glob 重跑。

> 不改 `DeviceContext` / `Context` —— 那是上层编排(guide §6-§7),本原语落地后它们才接入。
> 本文只交付「和 create_instance/select 同级的 create_device 原语 + 完整 DeviceCreateInfo」。

---

## 5. 一句话总结

`DeviceCreateInfo` 只需两样:**一个 `DeviceExtensionManifest` 指针**(扩展+特性一并复用 select
阶段那份)和**一份 `{family_index, queue_count}` 队列描述**(Context 用 QueueRole 算出的机械结果)。
`create_device` **不返回 queue 信息**——queue 句柄随 device 而生、`getQueue` 不会失败、且 Context
已掌握 `{family, count}`,让 DeviceContext 拿到 `vk::UniqueDevice` 后自己取 queue、套 QueueRole、
挂 TimelineSemaphore,GPU 模型完整留在 context 层,bootstrap 原语保持无状态、无 policy。




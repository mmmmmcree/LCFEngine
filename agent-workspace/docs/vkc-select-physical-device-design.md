# vkc `select_physical_device` 实现设计

设计 `libs/vk_core/include/vk_core/bootstrap/select_physical_device.h` 的实现，包括
`PhysicalDeviceSelectInfo` 的完整定义。一次性文档，看完即弃；代码是骨架，错误处理与
边界你按现有 `create_instance.cpp` 的 `_maythrow` / `noexcept` 模式补全。

参考锚点：
- `libs/vk_core/src/bootstrap/create_instance.cpp` —— `_maythrow` 私有实现 + `noexcept`
  边界 catch `vk::SystemError` 的既定模式，本文严格照搬。
- `libs/vk_core/include/vk_core/manifest/DeviceExtensionManifest.h` 与其 `.cpp` ——
  已实现的 `isRequiredFeaturesSupported(pd)` 与 `isExtensionRequired(name)`，本文直接复用。
- `agent-workspace/docs/vkc-device-queue-creation-guide.md` —— 上层 `DeviceContext::create`
  的编排逻辑（按角色选、去重、queue 解析）。本原语是它 §6.1 `evaluate_device` 那一步的底座。

---

## 0. 定位：这是「单次硬过滤 + 打分挑最优」的原语

`select_physical_device(instance, info)` 干一件事：枚举 instance 下所有物理设备，
**硬过滤**掉不满足要求的，对幸存者**打分**，返回分最高的那块 `vk::PhysicalDevice`。

它**不**做这些（都是上层 Context 的职责，见 guide §6）：

- 不认识 `DeviceRole` / `QueueRole`——它只认「一份要求」。
- 不做按角色去重、不建 `vk::Device`、不解析队列布局。
- **不碰 surface / window / present**（guide §0 核心不变量）。present-family 推迟到
  `Swapchain::create`。所以本原语的「队列要求」只表达 queue flag 能力，绝不查
  `getSurfaceSupportKHR`。

> 一句话：它和 `create_instance` 同级——一个无状态、可独立测试的 bootstrap 原语，
> 吃一个 `*SelectInfo`、返回一个 `expected`。

---

## 1. 三个维度的硬过滤

一块物理设备要通过，必须同时满足：

| 维度 | 数据源 | 怎么测 |
|---|---|---|
| API 版本 | `info.m_min_api_version` | `pd.getProperties().apiVersion >= min` |
| 设备扩展 | `DeviceExtensionManifest`（复用） | 逐个 `enumerateDeviceExtensionProperties` 名字命中 manifest |
| 设备特性 | `DeviceExtensionManifest`（复用） | `manifest.isRequiredFeaturesSupported(pd)`——已实现，内部 `queryFrom`+逐 bit test |
| 队列能力 | `info.m_required_queue_flags` | `getQueueFamilyProperties` 中存在某族覆盖每个要求的 flag |

特性维度**整段复用** `DeviceExtensionManifest::isRequiredFeaturesSupported`
（`manifest/DeviceExtensionManifest.cpp:7`），不重复造 probe 链逻辑——它已经按我们讨论过的
「现造 `PhysicalDeviceFeatureChain` → `queryFrom(pd)` → 逐个 `FeatureBit.test`」实现了。

扩展维度同样复用 manifest 的 `isExtensionRequired`，只是方向相反：select 这边遍历设备
**实际支持**的扩展、问 manifest「这个你要不要」，最后校验 manifest 要的每一个都被支持。

---

## 2. `PhysicalDeviceSelectInfo` 定义（替换 `create_infos.h:57-71` 的空壳）

现状是只有一个 `m_preffered_type` 的空壳（还拼错了 prefer）。下面是完整版，沿用本文件
既定的 `Self` 别名、删拷贝留移动、链式 setter 风格，与 `InstanceCreateInfo` 一致。

```cpp
// create_infos.h —— 替换现有 PhysicalDeviceSelectInfo
class PhysicalDeviceSelectInfo
{
    using Self = PhysicalDeviceSelectInfo;
public:
    ~PhysicalDeviceSelectInfo() noexcept;          // 出 .cpp（manifest 是不完整类型，见下）
    PhysicalDeviceSelectInfo() noexcept = default;
    PhysicalDeviceSelectInfo(const Self &) = delete;
    PhysicalDeviceSelectInfo(Self &&) noexcept = default;
    Self & operator =(const Self &) = delete;
    Self & operator =(Self &&) noexcept = default;
public:
    // 软偏好：设了且存在该类型的幸存者，就在该类型里选；否则全体里选（guide §6.1 软回退）。
    Self & setPreferredType(vk::PhysicalDeviceType type) noexcept
    {
        m_preferred_type = type;
        return *this;
    }
    // 硬过滤下限：默认 0（不限），可设 vk::ApiVersion13 等。
    Self & setMinApiVersion(uint32_t api_version) noexcept
    {
        m_min_api_version = api_version;
        return *this;
    }
    // 设备扩展 + 特性的要求，整份复用 manifest。可空（nullptr 表示不约束扩展/特性）。
    Self & setRequiredDeviceExtensionManifest(const DeviceExtensionManifest & manifest) noexcept;

    // 队列能力要求：必须存在某个 family 覆盖这些 flag。surface present 不在此列。
    Self & addRequiredQueueFlags(vk::QueueFlags flags) noexcept
    {
        m_required_queue_flags |= flags;
        return *this;
    }

    // —— 读侧（供 _maythrow 消费）——
    std::optional<vk::PhysicalDeviceType> getPreferredType() const noexcept { return m_preferred_type; }
    uint32_t getMinApiVersion() const noexcept { return m_min_api_version; }
    vk::QueueFlags getRequiredQueueFlags() const noexcept { return m_required_queue_flags; }
    const DeviceExtensionManifest * getDeviceExtensionManifest() const noexcept { return m_manifest_p; }

private:
    std::optional<vk::PhysicalDeviceType> m_preferred_type;
    uint32_t                              m_min_api_version = 0;
    vk::QueueFlags                        m_required_queue_flags = {};
    const DeviceExtensionManifest *       m_manifest_p = nullptr;
};
```

`create_infos.h` 顶部已前置声明 `class DeviceExtensionManifest;`（`:12`），所以存指针没问题。
`setRequiredDeviceExtensionManifest` 的实现和析构出 `create_infos.cpp`（与
`InstanceCreateInfo::setRequiredInstanceExtensionManifest` 同样的处理，`create_infos.cpp:8`）：

```cpp
// create_infos.cpp 追加
PhysicalDeviceSelectInfo::~PhysicalDeviceSelectInfo() noexcept = default;

auto PhysicalDeviceSelectInfo::setRequiredDeviceExtensionManifest(
    const DeviceExtensionManifest & manifest) noexcept -> Self &
{
    m_manifest_p = &manifest;
    return *this;
}
```

> 注意头文件需补 `#include <optional>`（已在 `create_infos.h:5`）和
> `#include <vulkan/vulkan.hpp>`（已在 `:3`，`vk::QueueFlags` / `vk::PhysicalDeviceType` 来自它）。
> 不要在头里 include `DeviceExtensionManifest.h`——保持前置声明 + 指针，编译墙更薄。

---

## 3. `select_physical_device.h`（签名不动）

现有签名已合适，保持：

```cpp
std::expected<vk::PhysicalDevice, std::error_code>
select_physical_device(vk::Instance instance, const PhysicalDeviceSelectInfo & info) noexcept;
```

---

## 4. `select_physical_device.cpp` 实现骨架

照搬 `create_instance.cpp` 的结构：`noexcept` 公开函数 catch `vk::SystemError`，核心逻辑放
匿名命名空间的 `_maythrow` 里。新增一个「无可用设备」的错误码。

```cpp
#include "vk_core/bootstrap/select_physical_device.h"
#include "vk_core/bootstrap/create_infos.h"
#include "vk_core/manifest/DeviceExtensionManifest.h"

namespace stdr = std::ranges;
namespace stdv = std::views;

namespace {

using namespace lcf::vkc;
using namespace lcf::vkc::bs;

// —— 单设备硬过滤 ——
bool passes_hard_filter(vk::PhysicalDevice pd, const PhysicalDeviceSelectInfo & info)
{
    // (1) API 版本
    auto props = pd.getProperties();
    if (props.apiVersion < info.getMinApiVersion()) { return false; }

    if (auto * manifest = info.getDeviceExtensionManifest()) {
        // (2) 设备扩展：manifest 要的每个都得被设备支持
        auto supported = pd.enumerateDeviceExtensionProperties();   // may throw
        // 把设备支持的扩展名灌进 set，再校验 manifest 要的每一个都在其中。
        // manifest 当前只暴露 isExtensionRequired(name)（方向：问它要不要），
        // 所以这里反过来收集设备支持集，交给上层/或给 manifest 补一个
        // requiredExtensions() 视图。最稳：给 manifest 加 const 访问器返回 required 集合，
        // 这里逐个 contains 校验（见 §6 落地清单）。
        for (const auto & required : manifest->requiredExtensions()) {  // ← §6 需补的访问器
            bool found = stdr::any_of(supported, [&](const vk::ExtensionProperties & e) {
                return required == e.extensionName.data();
            });
            if (not found) { return false; }
        }
        // (3) 设备特性：直接复用已实现的 probe+test
        if (not manifest->isRequiredFeaturesSupported(pd)) { return false; }
    }

    // (4) 队列能力：存在某族覆盖所有要求的 flag（不查 present）
    if (auto required = info.getRequiredQueueFlags()) {
        auto families = pd.getQueueFamilyProperties();
        bool ok = stdr::any_of(families, [&](const vk::QueueFamilyProperties & f) {
            return (f.queueFlags & required) == required;
        });
        if (not ok) { return false; }
    }
    return true;
}

// —— 打分：越大越好。只在通过硬过滤的设备间比较 ——
uint64_t score_device(vk::PhysicalDevice pd)
{
    auto props = pd.getProperties();
    uint64_t score = 0;
    // 独显基准分最高，其次集显；同档再按显存等加权（按需扩展）。
    switch (props.deviceType) {
        case vk::PhysicalDeviceType::eDiscreteGpu:   score += 1000; break;
        case vk::PhysicalDeviceType::eIntegratedGpu: score += 500;  break;
        case vk::PhysicalDeviceType::eVirtualGpu:    score += 250;  break;
        case vk::PhysicalDeviceType::eCpu:           score += 100;  break;
        default: break;
    }
    // 可叠加：本地显存堆大小、maxImageDimension2D 等。
    auto mem = pd.getMemoryProperties();
    for (uint32_t i = 0; i < mem.memoryHeapCount; ++i) {
        if (mem.memoryHeaps[i].flags & vk::MemoryHeapFlagBits::eDeviceLocal) {
            score += mem.memoryHeaps[i].size >> 20;   // MiB，作为细分权重
        }
    }
    return score;
}

vk::PhysicalDevice select_physical_device_maythrow(vk::Instance instance,
                                                   const PhysicalDeviceSelectInfo & info)
{
    struct Candidate { vk::PhysicalDevice pd; vk::PhysicalDeviceType type; uint64_t score; };
    std::vector<Candidate> candidates;
    for (auto pd : instance.enumeratePhysicalDevices()) {        // may throw
        if (not passes_hard_filter(pd, info)) { continue; }
        candidates.push_back({ pd, pd.getProperties().deviceType, score_device(pd) });
    }
    if (candidates.empty()) { return vk::PhysicalDevice{}; }     // 空句柄 → 边界层转错误码

    // 软类型偏好（guide §6.1）：设了偏好且有命中者，则只在命中者里挑；否则全体里挑。
    auto preferred = info.getPreferredType();
    auto matches = [&](const Candidate & c) { return preferred and c.type == *preferred; };
    bool has_preferred = preferred and stdr::any_of(candidates, matches);

    auto best = stdr::max_element(candidates, {}, [&](const Candidate & c) {
        int prefer = (has_preferred and matches(c)) ? 1 : 0;
        return std::pair{prefer, c.score};       // 字典序：先命中偏好，再 score
    });
    return best->pd;
}

} // namespace

namespace lcf::vkc::bs {

std::expected<vk::PhysicalDevice, std::error_code>
select_physical_device(vk::Instance instance, const PhysicalDeviceSelectInfo & info) noexcept
{
    vk::PhysicalDevice picked{};
    try {
        picked = select_physical_device_maythrow(instance, info);
    } catch (const vk::SystemError & e) {
        return std::unexpected(e.code());
    }
    if (not picked) {
        return std::unexpected(make_error_code(/* vkc::Error::eNoSuitablePhysicalDevice */));
    }
    return picked;
}

} // namespace lcf::vkc::bs
```

---

## 5. 设计要点与取舍

1. **特性/扩展复用 manifest，不重造。** `isRequiredFeaturesSupported` 已封装 probe 链
   全流程；select 只调用，杜绝两份 `queryFrom` 逻辑（正是前面几轮一直强调的「别再抄一份」）。

2. **软偏好 vs 硬过滤分明。** 类型偏好是排序键，不是过滤条件——设了独显但只有集显时回退到
   集显（guide §6.1 默认语义）。若某调用点要「偏好不满足即失败」的硬语义，让上层在拿到结果
   后自己校验 `getProperties().deviceType`，本原语不内建该分支，保持单一行为。

3. **queue 维度只测能力、不测 present。** `addRequiredQueueFlags` 只接受 `vk::QueueFlags`
   （graphics/compute/transfer）。surface present 支持是 per-(family, surface) 的运行期查询，
   按 guide §0 推迟到 swapchain 阶段——本原语连 `vk::SurfaceKHR` 参数都不收，从签名上杜绝。

4. **打分可独立演进。** `score_device` 是纯函数，后续要加「显存优先」「特定 vendor 优先」
   只改它，不动过滤与边界。默认 discrete > integrated > virtual > cpu，再用 device-local 堆
   大小细分同档。

5. **空结果用错误码，不用异常。** 与 `create_instance` 一致：`vk::SystemError`（驱动/枚举
   失败）在边界转 `std::error_code`；「枚举成功但无人通过过滤」是正常业务失败，单独一个
   `eNoSuitablePhysicalDevice` 码。需要一个 vkc 错误域（见落地清单）。

6. **与上层的衔接。** guide §6.1 的 `select_device_for_role` 现在用内联的 `evaluate_device`；
   落地后它应改为「构造一个 `PhysicalDeviceSelectInfo`（填该角色的 manifest + 偏好）→ 调
   `select_physical_device`」。本原语返回单块设备；「每角色选最优 + 去重 + 并集依赖」仍是
   Context 的编排，不下沉到这里。

---

## 6. 落地清单（仅列，不在本文改码）

1. **替换** `create_infos.h:57-71` 的 `PhysicalDeviceSelectInfo` 空壳为 §2 完整定义；
   修正字段名 `m_preffered_type` → `m_preferred_type`（顺手改拼写）。
2. **`create_infos.cpp`** 追加 `PhysicalDeviceSelectInfo` 的析构 + `setRequiredDeviceExtensionManifest`。
3. **`DeviceExtensionManifest`** 补一个 const 访问器暴露所需扩展集合，供过滤逐个校验：
   ```cpp
   const std::unordered_set<std::string> & requiredExtensions() const noexcept { return m_required_extensions; }
   ```
   （当前只有 `isExtensionRequired(name)`，方向反了，过滤侧需要正向遍历。）
4. **新建** `libs/vk_core/src/bootstrap/select_physical_device.cpp`，按 §4 实现。
5. **vkc 错误域**：定义 `enum class Error { eNoSuitablePhysicalDevice, ... }` + `std::error_category`
   + `make_error_code`，供本原语与后续 `create_device` 复用。（若已有错误域，直接挂一个码。）
6. **CMake** 无需改：`src/*.cpp` 是 `file(GLOB_RECURSE)`（`libs/vk_core/CMakeLists.txt:4`），
   新增 .cpp 自动纳入。重新 configure 一次让 glob 重跑。

---

## 7. 一句话总结

`select_physical_device` 是和 `create_instance` 同级的无状态 bootstrap 原语：吃
`PhysicalDeviceSelectInfo`（API 版本下限 + 软类型偏好 + queue flag 要求 + 复用的
`DeviceExtensionManifest`），硬过滤后按可演进的 `score_device` 挑最优，`expected` 返回。
特性/扩展校验全程复用 manifest 已实现的 probe+test，不查 surface/present，不认识 role——
角色编排与去重留在上层 Context。

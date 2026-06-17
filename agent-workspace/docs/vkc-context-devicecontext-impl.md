# vkc `Context` / `DeviceContext` 创建实现设计

落地 `libs/vk_core/src/context/Context.cpp` 与 `libs/vk_core/src/context/DeviceContext.cpp`
的创建逻辑。`Context` 用 `ContextCreateInfo` 聚合 `bs::` 的各种 info 把 instance + 每个
设备建起来、填 `m_device_context_table`;`DeviceContext` 自己选物理设备、按
`find_vacant_queue` 规则分配队列、填 `m_queue_context_table`——高端设备落到 3 个独立
family 各 1 条,低端全部别名到 graphics 队列。

**给文档,不改代码。** 代码片段是骨架,错误处理按 `create_instance.cpp` 的
`_maythrow` / `noexcept` 边界模式补全。

参考锚点:
- `libs/vk_core/src/bootstrap/{create_instance,select_physical_device,create_device}.cpp`
  —— 三个已实现、已在 `examples/vkc_test/main.cpp` 跑通的 bootstrap 原语。本文是它们的
  上层编排。
- `agent-workspace/docs/vkc-queue-allocation-reference.md` §5 —— `find_vacant_queue`
  分配算法的权威来源,本文 §4 直接落地它。
- `agent-workspace/docs/vkc-create-device-design.md` —— `DeviceCreateInfo` 收
  `{family,count}` 机械描述、create_device 不返回 queue 的结论。
- `libs/vk_core/include/vk_core/context/{Context,DeviceContext,QueueContext}.h` ——
  待填实现的三个类,签名已就位。

> ⚠️ 与旧 guide `vkc-device-queue-creation-guide.md` 的偏差:那份基于 `conf::FeatureDependency`
> 模型 + 「Context 先选物理设备再按 handle 去重」。当前代码已切到 **manifest 模型**
> (`InstanceExtensionManifest`/`DeviceExtensionManifest`),且 `DeviceContext::create`
> 签名是 **自包含选择**(收 instance + 自己 select)。本文以当前代码签名为准,旧 guide 的
> collapse ladder 表述以本文 §4 替代。

---

## 0. 定位与数据流

```
Context::create(ContextCreateInfo)                       ← 取出持有的 bs:: info,原样喂给原语
  ├─ instance = bs::create_instance(create_info.getInstanceCreateInfo())   → m_instance
  ├─ m_extension_resource_leases = manifest.enableExtensions(instance)
  └─ for role in DeviceRole:                              ← 填 m_device_context_table
       若该 role active(select info 已配)→ 新建 DeviceContext 并 create
       否则 → 别名到 eMain

DeviceContext::create(instance, DeviceContextCreateInfo)  ← 自包含:选设备 + 建 device + 建队列
  ├─ pd = bs::select_physical_device(instance, create_info.getPhysicalDeviceSelectInfo())  → m_physical_device
  ├─ families = pd.getQueueFamilyProperties()
  ├─ find_vacant_queue 阶梯 → {family→count} + {role→{family,index}}   (§4)
  ├─ device = bs::create_device(pd, device_info{manifest + {family,count}}) → m_device
  └─ 按 {family→count} 建 QueueContext;按 {role→{family,index}} 填 m_queue_context_table (§5)
```

核心不变量(贯穿全文):**不出现 surface / window / present 查询**。present-family 推迟到
`Swapchain::create`。所以 `select_physical_device` / 队列分配只看 queue flags。

---

## 1. 待定义的两个 CreateInfo —— 组合 `bs::` info,不重新包装

`Context.h:12` / `DeviceContext.h:13` 现在只前置声明了 `ContextCreateInfo` /
`DeviceContextCreateInfo`,**没有定义**。

**核心原则:组合,不重新包装。** `bs::InstanceCreateInfo` 已经把 `vk::ApplicationInfo`
**整个收下**(`setApplicationInfo(const vk::ApplicationInfo &)` 存一份),而不是把 app name /
version 拆开重新暴露一遍 setter。同理:

- `ContextCreateInfo` **直接持有一个 `bs::InstanceCreateInfo`**,不重新暴露 layers / app info /
  manifest 那一组 setter。
- `DeviceContextCreateInfo` **直接持有一个 `bs::PhysicalDeviceSelectInfo`**,不重新暴露
  preferred type / manifest。

`Context::create` 把持有的 `bs::` info **原样**喂给 `bs::create_instance` /
`bs::select_physical_device`,中间零重组。新增的 setter 只针对 `bs::` 原语**还没覆盖**的
上层概念(per-role 设备配置)。

### 1.1 `DeviceContextCreateInfo`(放 `context/DeviceContext.h`)

就是「一个 `bs::PhysicalDeviceSelectInfo` + 它是否被配置」。device 建立用的 manifest
从 select info 里**复用**(select 阶段用它硬过滤,create 阶段用它 enable,同一份)。

```cpp
// context/DeviceContext.h —— 在 DeviceContext 类之前
class DeviceContextCreateInfo
{
    using Self = DeviceContextCreateInfo;
public:
    ~DeviceContextCreateInfo() noexcept = default;
    DeviceContextCreateInfo() noexcept = default;
    DeviceContextCreateInfo(const Self &) = delete;            // select info 删了拷贝,跟着 move-only
    DeviceContextCreateInfo(Self &&) noexcept = default;
    Self & operator =(const Self &) = delete;
    Self & operator =(Self &&) noexcept = default;
public:
    // 直接收整个 select info(组合),不拆 preferred type / manifest 重新暴露。
    Self & setPhysicalDeviceSelectInfo(bs::PhysicalDeviceSelectInfo select_info) noexcept
    { m_select_info_opt = std::move(select_info); return *this; }

    const bs::PhysicalDeviceSelectInfo & getPhysicalDeviceSelectInfo() const noexcept { return *m_select_info_opt; }
    // 这个 role 是否被用户配置过 —— 决定 Context 给它建独立 DeviceContext 还是别名 eMain。
    bool isActive() const noexcept { return m_select_info_opt.has_value(); }
private:
    std::optional<bs::PhysicalDeviceSelectInfo> m_select_info_opt;
};
```

> `bs::PhysicalDeviceSelectInfo` 删拷贝留移动,所以 `std::optional<它>` 也 move-only,
> `DeviceContextCreateInfo` 跟着 move-only。`std::array<DeviceContextCreateInfo, N>` 默认构造
> +move-assign 进槽位都没问题。`isActive()` 用 `has_value()`——未配置的 role 槽位是空 optional。
> 头部需 `#include "vk_core/bootstrap/create_infos.h"`(要 `bs::PhysicalDeviceSelectInfo`
> 完整类型)、`<optional>`。

### 1.2 `ContextCreateInfo`(放 `context/Context.h`)

就是「一个 `bs::InstanceCreateInfo` + 按 `DeviceRole` 的 per-role 设备配置」。instance 那一组
参数全在 `bs::InstanceCreateInfo` 里,不重新暴露。per-role 用**显式命名 setter**(审计面,
cpp-style 约定),不做泛型 `setDeviceContextCreateInfo(role, info)`。

```cpp
// context/Context.h —— 在 Context 类之前
class ContextCreateInfo
{
    using Self = ContextCreateInfo;
    using DeviceContextCreateInfoArray = std::array<DeviceContextCreateInfo, enum_count_v<enums::DeviceRole>>;
public:
    ~ContextCreateInfo() noexcept = default;
    ContextCreateInfo() noexcept = default;
    ContextCreateInfo(const Self &) = delete;
    ContextCreateInfo(Self &&) noexcept = default;
    Self & operator =(const Self &) = delete;
    Self & operator =(Self &&) noexcept = default;
public:
    // 直接收整个 instance create info(组合),不拆 app info / layers / manifest 重新暴露。
    Self & setInstanceCreateInfo(bs::InstanceCreateInfo instance_create_info) noexcept
    { m_instance_create_info = std::move(instance_create_info); return *this; }

    // per-role 显式 setter,一个 DeviceRole 一个。加 role 时补一个对应 setter。
    Self & setMainDeviceContextCreateInfo(DeviceContextCreateInfo info) noexcept
    { m_device_context_create_infos[std::to_underlying(enums::DeviceRole::eMain)] = std::move(info); return *this; }
    Self & setComputeDeviceContextCreateInfo(DeviceContextCreateInfo info) noexcept
    { m_device_context_create_infos[std::to_underlying(enums::DeviceRole::eCompute)] = std::move(info); return *this; }

    const bs::InstanceCreateInfo & getInstanceCreateInfo() const noexcept { return m_instance_create_info; }
    const DeviceContextCreateInfo & getDeviceContextCreateInfo(enums::DeviceRole role) const noexcept
    { return m_device_context_create_infos[std::to_underlying(role)]; }
private:
    bs::InstanceCreateInfo m_instance_create_info;
    DeviceContextCreateInfoArray m_device_context_create_infos {};
};
```

> 头部需 `#include "context/DeviceContext.h"`(要 `DeviceContextCreateInfo` 完整类型做 array
> 成员;它已 include `bs::PhysicalDeviceSelectInfo`)。`enum_count_v` / `enums::DeviceRole` 已 include。
> `bs::InstanceCreateInfo` move-only → `ContextCreateInfo` 也 move-only,setter 按值收 + move。
> 调用方在外面把 debug utils 等 `register_*` 进 manifest、组好 `bs::InstanceCreateInfo`
> 和 `bs::PhysicalDeviceSelectInfo`,直接塞进来——与 `examples/vkc_test/main.cpp` 现有逐步写法
> 用的是**同一批** `bs::` info,只是改成由 ContextCreateInfo 持有。

---

## 2. 头文件联动改动

### 2.1 `DeviceContext.h`:`m_device` 改 `vk::UniqueDevice`

现状 `vk::Device m_device`(裸句柄)没有所有权——但 `bs::create_device` 返回
`vk::UniqueDevice`,device 的生命周期必须由 `DeviceContext` 持有。改:

```cpp
// 成员
vk::UniqueDevice m_device;                       // 从 vk::Device 改

// 访问器:UniqueDevice::get() 返回 vk::Device 值(句柄,廉价),不能再返回 const &
vk::Device getDevice() const noexcept { return m_device.get(); }   // 从 const vk::Device & 改
```

> `vk::Device` 是句柄(指针包装),按值返回廉价。`getPhysicalDevice()` 同理可保持
> `const vk::PhysicalDevice &` 或改按值,二选一统一即可。下游若有 `const vk::Device &` 绑定
> 处要跟着调整(当前 vk_core 内无此用法)。

### 2.2 `QueueContext.h`:补查 family/queue 的访问器(可选,建议)

`QueueContext::create` 已实现。建议补 `getFamilyIndex()`,future submit 漏斗会用。本阶段
非必须,可后补。

---

## 3. `Context::create` 实现

照 `create_instance.cpp` 结构:`noexcept` 公开边界。这里主要是取出持有的 `bs::` info、
原样喂给已 `noexcept` 的 bs 原语 + 填表,无裸 vk 抛出点,可直接写在 named ns。

```cpp
// src/context/Context.cpp
#include "vk_core/context/Context.h"
#include "vk_core/context/DeviceContext.h"
#include "vk_core/bootstrap/create_instance.h"
#include "vk_core/bootstrap/create_infos.h"
#include "vk_core/error.h"
#include <magic_enum/magic_enum.hpp>

namespace lcf::vkc {

std::error_code Context::create(const ContextCreateInfo & create_info) noexcept
{
    // 1. 持有的 bs::InstanceCreateInfo 原样喂给原语,零重组
    auto expected_instance = bs::create_instance(create_info.getInstanceCreateInfo());
    if (not expected_instance) { return expected_instance.error(); }
    m_instance = std::move(*expected_instance);

    // 2. 启用实例扩展(debug messenger 等),lease 存进 Context 维持其生命周期(见下方说明)
    if (auto * manifest = create_info.getInstanceExtensionEnableManifest()) {
        m_extension_resource_leases = manifest->enableExtensions(this->getInstance());
    }

    // 3. 每个 DeviceRole 建 DeviceContext + 填表
    return this->createDeviceContexts(create_info);
}

} // namespace lcf::vkc
```

> **扩展启用 lease 的归属——一个组合边界上的真实问题。** `bs::InstanceCreateInfo` 把
> manifest 存成 `const InstanceExtensionManifest *` 且**不对外暴露**;而
> `enableExtensions(instance)` 是**非 const**(它遍历 callback 注册 messenger 等,产出
> `ResourceLease`)。所以 Context **无法**透过 `getInstanceCreateInfo()` 拿到可变 manifest 来调
> enable。两个干净的解法,二选一:
>
> - **(A) `ContextCreateInfo` 另持一个非 const manifest 指针专供 enable**:
>   `setInstanceExtensionEnableManifest(InstanceExtensionManifest &)` →
>   `getInstanceExtensionEnableManifest()` 返回 `InstanceExtensionManifest *`。这**不违反组合**
>   ——它不是重新包装 instance 参数,而是补 `bs::InstanceCreateInfo` 语义上缺的「enable 阶段」
>   入口(create_instance 只读 manifest 做过滤,从不 enable)。上面骨架按 (A) 写。
> - **(B) 调用方自己 enable、把 lease 交给 Context**:`Context::create` 不碰 manifest,改提供
>   `setInstanceExtensionLeases(ResourceLeaseList)` 或建好后由调用方 `enableExtensions` 并自己
>   持有 lease。更纯,但把 lease 生命周期管理推给调用方,与「Context 已有 `m_extension_resource_leases`
>   成员」的现状意图相悖。
>
> 倾向 (A):messenger 这类 lease 必须和 instance 同寿,Context 持有 instance,理应持有 lease。
> 这一处需你拍板;它是 §1.2 之外唯一在 instance 侧的新增 setter。

`createDeviceContexts` 作为 `Context` 私有成员(在 `Context.h` 加声明
`std::error_code createDeviceContexts(const ContextCreateInfo &) noexcept;`):

```cpp
std::error_code Context::createDeviceContexts(const ContextCreateInfo & create_info) noexcept
{
    using enums::DeviceRole;
    // eMain 必配:它是所有未配置 role 的别名兜底
    if (not create_info.getDeviceContextCreateInfo(DeviceRole::eMain).isActive()) {
        return make_error_code(errc::main_device_role_not_configured);
    }
    m_device_contexts.reserve(enum_count_v<DeviceRole>);   // 防 vector 重分配使表指针失效

    std::array<DeviceContext *, enum_count_v<DeviceRole>> created {};
    for (auto role : magic_enum::enum_values<DeviceRole>()) {
        const auto & dev_info = create_info.getDeviceContextCreateInfo(role);
        if (not dev_info.isActive()) { continue; }         // 未配置 → 留给下面别名
        auto ctx = std::make_unique<DeviceContext>();
        if (auto ec = ctx->create(this->getInstance(), dev_info)) { return ec; }
        m_device_contexts.push_back(std::move(ctx));
        created[std::to_underlying(role)] = m_device_contexts.back().get();
    }

    // 填表:配置过的指自己,没配置的别名 eMain
    DeviceContext * main_ctx = created[std::to_underlying(DeviceRole::eMain)];
    for (auto role : magic_enum::enum_values<DeviceRole>()) {
        auto idx = std::to_underlying(role);
        m_device_context_table[idx] = created[idx] ? created[idx] : main_ctx;
    }
    return {};
}
```

> **物理设备去重是本阶段非目标。** 当前签名下每个 active role 自包含地 select + create 一个
> 独立 `vk::Device`。单 GPU 机器只配 eMain,eCompute 别名 eMain——不会重复建 device。
> 只有当用户**显式**配置 eCompute(异构多 GPU offload,通常配不同 preferred type)时才建第二个
> device。若两个 active role 软回退到同一物理设备,会建两个逻辑设备(不共享资源)——这是已知
> 边界,留到将来真有需求时再加「按 `VkPhysicalDevice` handle 去重」(需把 select 上提到 Context,
> 改 `DeviceContext::create` 签名)。本阶段不做。

---

## 4. `DeviceContext::create` —— 选设备 + 队列分配(核心)

```cpp
// src/context/DeviceContext.cpp
#include "vk_core/context/DeviceContext.h"
#include "vk_core/context/QueueContext.h"
#include "vk_core/bootstrap/select_physical_device.h"
#include "vk_core/bootstrap/create_device.h"
#include "vk_core/bootstrap/create_infos.h"
#include "vk_core/manifest/DeviceExtensionManifest.h"
#include "vk_core/error.h"
#include "vk_core/context/enums.h"     // queue_role_flavor
#include <magic_enum/magic_enum.hpp>
#include <map>

namespace stdr = std::ranges;

namespace {
using namespace lcf::vkc;
struct QueueAssignment { uint32_t family; uint32_t index; };
}
```

### 4.1 选物理设备

`DeviceContextCreateInfo` 持有的 `bs::PhysicalDeviceSelectInfo` **原样**喂给原语,零重组:

```cpp
std::error_code DeviceContext::create(vk::Instance instance, const DeviceContextCreateInfo & create_info) noexcept
{
    auto expected_pd = bs::select_physical_device(instance, create_info.getPhysicalDeviceSelectInfo());
    if (not expected_pd) { return expected_pd.error(); }
    m_physical_device = *expected_pd;
    // ... §4.2 分配队列 → §4.3 建 device → §5 建 QueueContext
}
```

> **manifest 复用——又一个组合边界。** `bs::create_device` 要同一份 `DeviceExtensionManifest`
> 来 enable 扩展+特性,而它正存在 `PhysicalDeviceSelectInfo` 里(select 阶段做硬过滤用)。但
> `PhysicalDeviceSelectInfo` 当前**不暴露** `m_extension_manifest_p`。为了让 `DeviceContext`
> 把它转交给 `DeviceCreateInfo`,给 `PhysicalDeviceSelectInfo` 补一个 getter:
> `const DeviceExtensionManifest * getDeviceExtensionManifest() const noexcept`。这是纯读
> 访问器,与既有 `getRequiredQueueFlags()` / `getPreferredTypeOptional()` 同形,不破坏封装。
> §4.3 据此把同一份 manifest 喂给 `create_device`——「select 与 create 复用同一 manifest」正是
> `vkc-create-device-design.md` §1 的结论。

### 4.2 `find_vacant_queue` 阶梯(权威算法,见 reference §5)

`families` 是 `getQueueFamilyProperties()` 的**可变副本**——`queueCount` 会被递减占位。

```cpp
auto families = m_physical_device.getQueueFamilyProperties();
std::vector<uint32_t> queue_offsets(families.size(), 0);   // family → 已领 queue 数 = 要建几条 = 下一个可领 index

auto find_vacant_queue = [&](QueueAssignment & out, vk::QueueFlags required, vk::QueueFlags ignore) -> bool {
    for (uint32_t i = 0; i < families.size(); ++i) {
        if (families[i].queueFlags & ignore) { continue; }
        if (families[i].queueCount == 0) { continue; }
        if ((families[i].queueFlags & required) != required) { continue; }
        out = { i, queue_offsets[i] };
        --families[i].queueCount;
        ++queue_offsets[i];
        return true;
    }
    return false;
};

using QF = vk::QueueFlagBits;
std::array<std::optional<QueueAssignment>, enum_count_v<enums::QueueRole>> role_queues;
auto slot = [](enums::QueueRole r) { return std::to_underlying(r); };

// graphics:GRAPHICS|COMPUTE 的族(无则硬错误)。不查 present。
QueueAssignment graphics;
if (not find_vacant_queue(graphics, QF::eGraphics | QF::eCompute, {})) {
    return make_error_code(errc::no_suitable_queue_family);
}
role_queues[slot(enums::QueueRole::eGraphics)] = graphics;

// compute:专用 async(COMPUTE 非 GRAPHICS)→ 任意 COMPUTE → 别名 graphics
QueueAssignment compute;
if (find_vacant_queue(compute, QF::eCompute, QF::eGraphics) or
    find_vacant_queue(compute, QF::eCompute, {})) {
    role_queues[slot(enums::QueueRole::eCompute)] = compute;
} else {
    role_queues[slot(enums::QueueRole::eCompute)] = graphics;     // 别名
}

// transfer:专用 DMA(TRANSFER 非 GRAPHICS|COMPUTE)→ compute 族 → 别名 compute
QueueAssignment transfer;
auto compute_q = *role_queues[slot(enums::QueueRole::eCompute)];
if (find_vacant_queue(transfer, QF::eTransfer, QF::eGraphics | QF::eCompute) or
    find_vacant_queue(transfer, QF::eCompute, QF::eGraphics)) {
    role_queues[slot(enums::QueueRole::eTransfer)] = transfer;
} else {
    role_queues[slot(enums::QueueRole::eTransfer)] = compute_q;   // 别名 compute
}
```

产物两份:`queue_offsets`(family → 建几条)+ `role_queues`(role → `{family,index}`,可别名)。

### 4.3 建 device

manifest 从持有的 select info 里取(§4.1 补的 getter),与 select 阶段复用同一份:

```cpp
bs::DeviceCreateInfo device_info;
if (const auto * manifest = create_info.getPhysicalDeviceSelectInfo().getDeviceExtensionManifest()) {
    device_info.setRequiredDeviceExtensionManifest(*manifest);
}
for (uint32_t fam = 0; fam < queue_offsets.size(); ++fam) {
    if (queue_offsets[fam] > 0) { device_info.addQueueFamilyRequest(fam, queue_offsets[fam]); }
}
auto expected_device = bs::create_device(m_physical_device, device_info);
if (not expected_device) { return expected_device.error(); }
m_device = std::move(*expected_device);   // m_device 是 vk::UniqueDevice(见 §2.1)
```

---

## 5. 建 `QueueContext` + 填 `m_queue_context_table`

每个 `{family,index}` 只建一个 `QueueContext`(别名的 role 共享它)。用 map 去重 `{family,index}`。

```cpp
m_queue_contexts.reserve(/* 总 queue 数 = sum(queue_offsets) */);
std::map<std::pair<uint32_t, uint32_t>, QueueContext *> queue_lookup;

for (uint32_t fam = 0; fam < queue_offsets.size(); ++fam) {
    for (uint32_t i = 0; i < queue_offsets[fam]; ++i) {
        auto qc = std::make_unique<QueueContext>();
        if (auto ec = qc->create(this->getDevice(), fam, i)) { return ec; }
        m_queue_contexts.push_back(std::move(qc));
        queue_lookup[{fam, i}] = m_queue_contexts.back().get();
    }
}

for (auto role : magic_enum::enum_values<enums::QueueRole>()) {
    auto idx = std::to_underlying(role);
    const auto & assign = role_queues[idx];          // 核心 3 role 一定有值
    m_queue_context_table[idx] = queue_lookup.at({assign->family, assign->index});
}
return {};
```

> `m_queue_contexts` 是 `vector<unique_ptr<QueueContext>>`——元素是堆对象,`push_back` 重分配
> 不会使已存的 `QueueContext *` 失效(失效的是 unique_ptr 的容器槽,不是它指向的对象)。所以
> `queue_lookup` 存的裸指针安全。`reserve` 仍建议加(省重分配)。别名安全:多个 role 指向同一
> `QueueContext`,共享唯一提交漏斗 + TimelineSemaphore,不破坏同步。

---

## 6. 两个设备走查(验证算法落点)

**NV 5080**(6 family:f0=`G|C|T`×16、f1=`T`×2、f2=`C|T`×8、f3=`T|Decode`、f4=`T|Encode`、f5=`T|OpticalFlow`):

| role | find_vacant_queue | 落到 |
|---|---|---|
| eGraphics | `G\|C` → f0,f0.count 16→15 | {0,0} |
| eCompute | `C` ignore `G` → f2(专用) | {2,0} |
| eTransfer | `T` ignore `G\|C` → f1(专用 DMA) | {1,0} |

`queue_offsets` = {f0:1, f1:1, f2:1} → 建 3 条 queue,3 个独立 family 各 1 条。video/optical-flow
的 family 不被触碰。**符合「高端 3 family 各 1 queue」。**

**低端 iGPU**(单 family f0=`G|C|T`,queueCount=1):

| role | find_vacant_queue | 落到 |
|---|---|---|
| eGraphics | `G\|C` → f0,f0.count 1→0 | {0,0} |
| eCompute | 专用无;任意 `C` → f0 但 count=0 失败 → 别名 graphics | {0,0} |
| eTransfer | 专用无;compute 族 f0 count=0 失败 → 别名 compute | {0,0} |

`queue_offsets` = {f0:1} → 建 1 条 queue,3 个 role 全别名到它。**符合「低端全部映射到 graphics 队列」。**

---

## 7. 落地清单(从底向上,每步可独立编译)

1. **`bootstrap/create_infos.h`** —— 给 `PhysicalDeviceSelectInfo` 补只读 getter
   `const DeviceExtensionManifest * getDeviceExtensionManifest() const noexcept`(§4.1),
   让 `DeviceContext` 能把 select 阶段的 manifest 转交 `create_device`。纯访问器,与现有
   `getRequiredQueueFlags()` 同形。
2. **`context/DeviceContext.h`** —— 加 `DeviceContextCreateInfo` 定义(§1.1,持有
   `optional<bs::PhysicalDeviceSelectInfo>`);`m_device` 改 `vk::UniqueDevice`、`getDevice()`
   返回 `vk::Device` 值(§2.1);`#include "vk_core/bootstrap/create_infos.h"`、`<optional>`。
3. **`context/Context.h`** —— 加 `ContextCreateInfo` 定义(§1.2,持有 `bs::InstanceCreateInfo` +
   per-role `DeviceContextCreateInfo` 数组 + 可选的 enable-manifest 指针,见 §3 (A));`Context`
   加私有 `createDeviceContexts` 声明;`#include "context/DeviceContext.h"`。
4. **`src/context/DeviceContext.cpp`** —— §4 + §5。文件布局按 cpp-style:顶部匿名 ns 放
   `QueueAssignment` / helper 声明,中部 named ns 放 `DeviceContext::create`。
5. **`src/context/Context.cpp`** —— §3 的 `Context::create` + `createDeviceContexts`。
6. **CMake 无需改**(`src/**/*.cpp` 是 GLOB);重新 configure 一次让 glob 重跑。
7. **`examples/vkc_test/main.cpp`(可选)** —— 把现有逐步 bootstrap 改写成:组好
   `bs::InstanceCreateInfo` / `bs::PhysicalDeviceSelectInfo` → 塞进 `ContextCreateInfo` →
   `Context::create` 一把建起来,对照验证组合路径。

> 不依赖旧 guide 的 `conf::FeatureDependency` / `flatten_dependencies` / `evaluate_device`——
> 那套在 manifest 模型下不存在。manifest 已在 `select_physical_device` 内部做完硬过滤
> (`isRequiredFeaturesSupported` + 扩展计数),`DeviceContext` 不重做评估。

---

## 8. 一句话总结

`ContextCreateInfo` / `DeviceContextCreateInfo` **组合** `bs::InstanceCreateInfo` /
`bs::PhysicalDeviceSelectInfo`(不重新包装 app info / layers / manifest / preferred type),
新增 setter 只补 `bs::` 原语没覆盖的上层概念(per-role 设备配置、enable manifest)。
`Context::create` 把持有的 `bs::InstanceCreateInfo` 原样喂 `bs::create_instance`、启用扩展、
再对每个 **active** `DeviceRole` 建一个自包含 `DeviceContext`(未配置的别名 eMain),填
`m_device_context_table`。`DeviceContext::create` 把持有的 `bs::PhysicalDeviceSelectInfo`
原样喂 `bs::select_physical_device`、用 `find_vacant_queue` 阶梯算出 `{family→count}` 与
`{role→{family,index}}`、复用同一 manifest 喂 `bs::create_device`、再按 family×count 建
`QueueContext` 并按 role 填 `m_queue_context_table`——高端落到 3 个独立 family 各 1 条,低端
全部别名到 graphics 队列。物理设备去重是本阶段非目标。

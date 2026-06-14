# vkc Feature Manifest:模块注册形态 / 类型拆分 / Chain 归宿

> 设计文档，回答三个问题：①各模块 `register_feature_xxx` 怎么写；②是否拆成
> `InstanceFeatureManifest` + `DeviceFeatureManifest`；③`PhysicalDeviceFeatureChain`
> 放哪、还是直接用 `FeatureManifest`。一次性文档，看完即弃。

## §0 直接结论(TL;DR)

1. **拆成两个类**:`InstanceFeatureManifest`(全局一个)+ `DeviceFeatureManifest`
   (每个 DeviceRole 一个)。理由:instance 作用域天然单数、device 作用域天然多数,
   这是 Vulkan 对象的客观事实,不是风格选择。拆开后**实例扩展不再需要"跨角色求并集"**
   ——模块直接写进那唯一的 instance manifest。(单一 manifest 方案才需要 union,见 §7)

2. **`PhysicalDeviceFeatureChain` 删掉**。它是 `conf::FeatureChain` 的副本(只少了
   `queryFrom`)。feature chain 归 `DeviceFeatureManifest` 私有持有,且必须用
   `conf::FeatureChain`——因为筛选物理设备时要 `queryFrom(pd)` 测支持。当前
   `FeatureManifest::enableFeature` 手写 chain 突变、绕开 `k_feature.enable`,
   正是"持有了错误的 chain 类型"的症状。

3. **create_xxx 直接吃 manifest,但只吃 feature 这一片**。manifest 提供
   "需要什么能力(layers / extensions / features / 回调)";其余维度(app_info、
   选设备策略、队列布局)仍走独立显式参数。manifest 不做上帝对象。

4. **三层不变**:`conf::` 静态声明层(`FeatureDependency` / `k_feature` / `FeatureBit`)
   原样保留。manifest 是它之上的**运行期聚合层**,`register_feature_xxx` 把模块的
   constexpr `k_module_dependency` 喂进 manifest——constexpr 机制一点不丢。

## §1 当前代码:三个东西在打架

读了一遍现状,feature 相关的"chain"出现了三份,职责重叠:

| 符号 | 位置 | 实质 | 问题 |
|---|---|---|---|
| `conf::FeatureChain` | `config/FeatureDeclare.h` | 运行期 pNext 链,**有 `queryFrom`** | 正主 |
| `conf::FeatureDependency` / `k_feature` / `FeatureBit` | 同上 | constexpr 静态声明 | 正主,保留 |
| `vkc::PhysicalDeviceFeatureChain` | `config/PhysicalDeviceFeatureChain.h` | `conf::FeatureChain` 的副本,**缺 `queryFrom`** | 冗余,删 |
| `vkc::FeatureManifest` | `config/FeatureManifest.h` | 字符串集 + 上面那个副本 chain + 回调 | 形态待定 |

两个隐藏 bug,顺手记一下(本文不改码):
- `FeatureManifest.h:33` `using Feature = ...::class_type` 缺分号。
- `FeatureManifest::enableFeature` 手写 `m_feature_chain.request<Feature>().*member = true`,
  这是把 `k_feature.enable` 又抄了一遍。根因是它持有 `PhysicalDeviceFeatureChain`
  而非 `conf::FeatureChain`,导致没法直接复用 `FeatureBit`。

## §2 为什么拆成两个类(回答你的问题①)

你的直觉对:**两个 manifest**,但分界线不是"风格",是 Vulkan 对象的作用域。

```
              instance 作用域           device 作用域
              ─────────────────         ─────────────────
对象数量       全程 1 个 vk::Instance     每个 DeviceRole 一个 vk::Device
内容           instance layers           device extensions
              instance extensions        PhysicalDeviceFeatures(chain)
              messenger 等实例级资源       device 级资源
回调时机       after-instance            after-device
```

- 一个 instance、多个 device,这是硬事实。
- eMain 要 surface 扩展、eCompute 不要——所以 **device 侧依赖必须按 role 分别收集**。
- 但 instance 扩展只有一份。如果用单一大 manifest 装所有 role 的东西(我上一篇
  feature-manifest-design 描述的形态),Context 就得"把每个 role manifest 的 instance
  扩展求并集"。**拆成两个类直接消灭这个 union**:模块写 instance 扩展时写进那唯一的
  `InstanceFeatureManifest`,写 device 扩展/feature 时写进当前 role 的
  `DeviceFeatureManifest`。

```cpp
namespace lcf::vkc {

// 全局唯一。Context 持有一个。
class InstanceFeatureManifest
{
    using Self = InstanceFeatureManifest;
    using InstanceResourceFn = std::function<ResourceLease(vk::Instance)>;
public:
    Self & addLayer(std::string_view) noexcept;
    Self & addExtension(std::string_view) noexcept;
    Self & addExtensions(convertible_range_of_c<std::string> auto &&) noexcept;
    // after-instance 回调:messenger 这类实例级资源
    Self & addResourceCallback(InstanceResourceFn) noexcept;

    bool requiresLayer(const std::string &) const noexcept;
    bool requiresExtension(const std::string &) const noexcept;
    std::span<const std::string> layers() const noexcept;     // create_instance 读
    std::span<const std::string> extensions() const noexcept; // create_instance 读
    std::vector<ResourceLease> enableResources(vk::Instance) noexcept; // Context 建完调

private:
    std::vector<std::string> m_layers;
    std::vector<std::string> m_extensions;
    std::vector<InstanceResourceFn> m_resource_callbacks;
};

// 每个 DeviceRole 一个。Context 为每个角色各持一份。
class DeviceFeatureManifest
{
    using Self = DeviceFeatureManifest;
    using DeviceResourceFn = std::function<ResourceLease(vk::Device)>;
public:
    Self & addExtension(std::string_view) noexcept;
    Self & addExtensions(convertible_range_of_c<std::string> auto &&) noexcept;

    // 静态声明喂进来:模块的 constexpr k_module_dependency
    Self & require(const conf::FeatureDependency & dep) noexcept; // 展开 dep,见 §4
    Self & addResourceCallback(DeviceResourceFn) noexcept;

    // —— 物理设备筛选期用 ——
    void buildEnableChain(conf::FeatureChain & out) const;  // 把要的 feature .enable 进 out
    bool isSupportedBy(vk::PhysicalDevice pd) const;        // queryFrom + 每个 FeatureBit.test

    std::span<const std::string> extensions() const noexcept;
    std::vector<ResourceLease> enableResources(vk::Device) noexcept;

private:
    std::vector<std::string> m_extensions;
    std::vector<conf::FeatureBit> m_feature_bits; // 来自 require() 展开,延迟到选设备时再建 chain
    std::vector<DeviceResourceFn> m_resource_callbacks;
};

} // namespace lcf::vkc
```

> 关键点:`DeviceFeatureManifest` **不持有 chain 实例**,只持有 `FeatureBit` 列表。
> chain 是"针对某个候选物理设备"的临时品,选设备时才 `buildEnableChain` + `queryFrom`
> 现造现测。一个 manifest 要拿去测多块物理设备(你有独显+集显),持久 chain 反而碍事。
> 这正是 §3 删 `PhysicalDeviceFeatureChain` 的根据。

## §3 `PhysicalDeviceFeatureChain` 放哪 —— 答:删掉(回答你的问题②)

它就是 `conf::FeatureChain` 抄掉 `queryFrom` 的版本。而筛设备恰恰**需要**
`queryFrom`。所以:

- 删 `config/PhysicalDeviceFeatureChain.h`。
- 任何要 chain 的地方一律用 `conf::FeatureChain`(它在 `FeatureDeclare.h`,本就是
  `k_feature` 的搭档)。
- `FeatureManifest` 当前 `#include "PhysicalDeviceFeatureChain.h"` 那行去掉。

为什么不让 manifest 持有一个 `conf::FeatureChain` 成员?因为 chain 是 per-物理设备
的临时态:

```cpp
// select_physical_device 内部,对每个候选 pd:
conf::FeatureChain wanted;            // 我要开的
role_manifest.buildEnableChain(wanted);   // 把 FeatureBit.enable 灌进去
conf::FeatureChain probe;             // 探测这块设备实际支持什么
role_manifest.buildEnableChain(probe);    // 同结构(pNext 链节点齐全)
probe.queryFrom(pd);                  // 让驱动填实际支持
bool ok = role_manifest.allBitsSupported(probe); // 每个 FeatureBit.test(probe)
```

manifest 持有 `FeatureBit` 列表(声明"我要什么"),chain 是针对具体设备的运行期产物。
两者生命周期不同,不该塞一起。`enableFeature<member>` 这个接口也随之删掉——改成
模块通过 `require(k_module_dependency)` 批量喂(§4),不再逐个手点 chain。

## §4 各模块 `register_feature_xxx` 怎么写(回答你的问题①核心)

模块的 `feature_dependencies.h` **保持现状**(constexpr `k_module_dependency` 不动),
只**新增**一个注册函数。注册函数干两件事:把静态 dep 喂给 device manifest +(如果有)
注册运行期回调。

**A. 纯声明型模块(sync —— 只有 feature,无回调)**

```cpp
// sync/feature_dependencies.h ,在现有 k_module_dependency 之后追加:
inline void register_feature_sync(DeviceFeatureManifest & device) noexcept
{
    device.require(k_module_dependency); // 展开 device_extensions + features
}
```

`require(dep)` 的展开逻辑(manifest 内部):
```cpp
DeviceFeatureManifest & DeviceFeatureManifest::require(const conf::FeatureDependency & dep) noexcept
{
    for (auto * ext : dep.device_extensions) { addExtension(ext); }
    for (auto & bit : dep.features) { m_feature_bits.push_back(bit); }
    for (auto * opt : dep.optional) { require(*opt); } // 递归可选依赖(策略另议)
    // dep.instance_extensions 不归 device manifest 管 —— 见下 surface
    return *this;
}
```

**B. 跨作用域模块(surface —— 既有 instance 扩展又有 device 扩展/feature)**

surface 的 `k_module_dependency` 同时含 `instance_extensions` 和 `device_extensions`。
注册函数因此要两个 manifest:

```cpp
// surface/feature_dependencies.h 追加:
inline void register_feature_surface(InstanceFeatureManifest & instance,
                                     DeviceFeatureManifest & device) noexcept
{
    for (auto * ext : surf::k_module_instance_extensions) { instance.addExtension(ext); }
    device.require(k_module_dependency); // 只取 device 那一片 + optional
}
```

**C. 带运行期资源的模块(debug_utils —— instance 回调)**

debug_utils 今天散在两处:扩展名靠用户手填、`enable_debug_utils` 是独立函数。
统一成一个注册函数:

```cpp
// 新增 details/instance_extensions/debug_utils 的声明区:
inline void register_feature_debug_utils(InstanceFeatureManifest & instance) noexcept
{
    instance.addExtension(vk::EXTDebugUtilsExtensionName);
    instance.addLayer("VK_LAYER_KHRONOS_validation"); // 或 vk:: 已定义版本
    instance.addResourceCallback(&enable_debug_utils); // 现有函数直接当回调
}
```

`enable_debug_utils(vk::Instance) -> ResourceLease` 签名已经吻合 `InstanceResourceFn`,
零改动接进来。

> 形态统一为:**每个模块一个 `register_feature_xxx(manifest...)`,参数表"按它触及的
> 作用域"决定要 instance 还是 device 还是两者**。不强行让所有模块签名一致——surface
> 客观上跨两个作用域,签名就该有两个参数。这是诚实,不是不统一。

## §5 是否"直接用 FeatureManifest 完成注册 + create_xxx"(回答你的问题③)

你问的"还是说直接使用 FeatureManifest 来完成注册以及 create_xxx 时使用"——
**是的,就是这个用法,只是把"一个 FeatureManifest"换成"两个职责清晰的 manifest"**。
注册期写 manifest,创建期 create_xxx 读 manifest:

```cpp
// 创建期 create_instance 只读 InstanceFeatureManifest 的 layers()/extensions():
std::expected<vk::UniqueInstance, std::error_code>
create_instance(const InstanceCreateInfo & info, const InstanceFeatureManifest & manifest) noexcept;

// select / create_device 只读 DeviceFeatureManifest:
std::expected<vk::PhysicalDevice, std::error_code>
select_physical_device(vk::Instance, const PhysicalDeviceSelectInfo &, const DeviceFeatureManifest &) noexcept;

std::expected<vk::UniqueDevice, std::error_code>
create_device(vk::PhysicalDevice, const DeviceCreateInfo &, const DeviceFeatureManifest &) noexcept;
```

注意 create_xxx **只读 manifest 的 feature 这一片**(layers/extensions/features/chain),
不碰回调——回调时机晚于创建,由 Context 在拿到 instance/device **之后**调
`manifest.enableResources(handle)`。这维持了上一篇分层文档的边界:**details 原语不认识
DeviceRole/QueueRole**,它只认识"一份需要什么能力的清单"。

`InstanceCreateInfo` 那个 `requiresLayer` 查错集合的 bug(查了
`m_required_instance_extensions`)在新形态下自然消失——layer 查询走
`InstanceFeatureManifest::requiresLayer`,只有一个 layer 集合可查。

## §6 完整调用流程(精炼版)

```cpp
// 1) 实例级:全局一个
InstanceFeatureManifest inst_manifest;
register_feature_debug_utils(inst_manifest);        // 想要哪个 feature 就 include + 注册哪个
register_feature_surface(inst_manifest, main_dev_manifest); // surface 跨两边

// 2) 设备级:每个 DeviceRole 一个
DeviceFeatureManifest main_dev_manifest;   // eMain
DeviceFeatureManifest compute_dev_manifest;// eCompute
register_feature_sync(main_dev_manifest);
register_feature_sync(compute_dev_manifest);
register_feature_surface(inst_manifest, main_dev_manifest); // 只有 eMain 要 surface

// 3) 创建实例,读 inst_manifest
auto instance = create_instance(inst_info, inst_manifest).value();

// 4) Context 在拿到 instance 后,跑 after-instance 回调(messenger 在此诞生)
auto instance_leases = inst_manifest.enableResources(*instance);

// 5) 每个 role:选物理设备(用该 role 的 device_manifest 硬筛 feature/extension 支持)
auto pd = select_physical_device(*instance, select_info, main_dev_manifest).value();

// 6) 建 device,读该 role 的 device_manifest(extensions + 把 features 灌进 DeviceCreateInfo 的 chain)
auto device = create_device(pd, dev_info, main_dev_manifest).value();

// 7) Context 跑 after-device 回调
auto device_leases = main_dev_manifest.enableResources(*device);
```

(注册顺序示意,实际可调;`register_feature_surface` 在 1 和 2 都出现是因为它跨作用域,
两次调用各喂一边——或拆成 `register_surface_instance` / `register_surface_device` 两个,
看你偏好。)

## §7 权衡表

| 维度 | 单一 `FeatureManifest`(上一篇形态) | 双 manifest(本篇) |
|---|---|---|
| instance 扩展去重 | Context 跨 role 求**并集** | 天然单数,**零 union** |
| 回调时机 | 一个桶,要靠 phase 字段区分 | 类型即时机:Instance/Device 各一类 |
| 误用风险 | device 扩展可能误写进"全局"被当 instance 扩展 | 编译期就分流:写错对象编译不过 |
| 类数量 | 1 | 2(+ 删掉 1 个 chain 副本,净 +0) |
| 与 `conf::` 层 | 包一层 | 同样包一层,机制不丢 |
| create_xxx 签名 | 吃一个大 manifest | 吃对应作用域 manifest,更窄 |

双 manifest 唯一"成本":surface 这种跨作用域模块的注册函数有两个参数。但这恰好把
"surface 同时影响实例和设备"这个事实显式化,反而是优点。

## §8 落地清单(本文不改码,仅列)

1. 删 `config/PhysicalDeviceFeatureChain.h`;`FeatureManifest.h` 去掉对应 include。
2. `FeatureManifest.h` 拆成 `InstanceFeatureManifest` + `DeviceFeatureManifest`
   (可仍放同一头文件,或拆两个文件)。
3. `DeviceFeatureManifest` 持有 `std::vector<conf::FeatureBit>`,提供 `require(dep)` /
   `buildEnableChain(conf::FeatureChain&)` / `isSupportedBy(pd)`;**不**持久持有 chain。
4. 删 `enableFeature<member>` 手写突变,改 `require(k_module_dependency)`。
5. 各模块 `feature_dependencies.h` 追加 `register_feature_xxx`;constexpr dep 不动。
6. debug_utils 的 `enable_debug_utils` 当 `InstanceResourceFn` 接入(签名已吻合)。
7. create_instance / select_physical_device / create_device 各加一个 manifest 参数,
   只读 feature 片;回调由 Context 创建后调。
8. 修 `requiresLayer` 查错集合的 bug(双 manifest 后自然只剩一个 layer 集)。


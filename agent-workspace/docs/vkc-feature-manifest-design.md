# vkc FeatureManifest:统一注册面 —— 设计评估

评估「用 `config/FeatureManifest.h` 一个类,各模块 `feature_deps.h` 提供
`register_feature_xxx(FeatureManifest&)`,create_* 函数读 manifest」是否比当前
`constexpr conf::FeatureDependency` 更优雅。一次性文档,用完即弃。给文档不改代码。

背景:ADR-0001 已拒绝 include-driven 自动注册,选「显式传依赖」。本提议是显式调用
`register_feature_xxx`,**不违反** ADR-0001(显式,非 include 即注册)。

---

## 0. 结论

**在一个维度上更优雅:它把今天割裂的两套机制合并成单一注册面。** 但要守住三条:

1. **静态 feature bits 的 constexpr 机制不能丢**(`k_feature<&member>` NTTP)。manifest 不是
   替代 `FeatureDependency`,是补上它表达不了的那一半(运行期资源回调)。→ 分层,不二选一(§2)。
2. **回调有"阶段"** —— after-instance / after-device 是不同时机、不同 owner。扁平装一起会乱序。
   → 回调按 phase 标签组织,create_* 各读自己那片(§3)。
3. **manifest 不是单数** —— 实例级 manifest(全角色并集)vs 每个 DeviceRole 的设备级 manifest
   不是同一个。你描述的「单一 manifest」要拆成 instance scope + per-role device scope(§4)。

---

## 1. 它真正解决的问题:合并两套并行机制

今天一个 feature(以 debug_utils 为例)的信息散在两处性质不同的地方:

| | 内容 | 现在在哪 | 性质 |
|---|---|---|---|
| 静态选择数据 | 扩展名、feature bits | `conf::FeatureDependency`(constexpr);debug_utils 甚至没有,靠用户在 main.cpp 手动 `addRequiredInstanceExtension` | 编译期静态 |
| 运行期资源回调 | "instance 建好后建 messenger,返回 lease" | `details::enable_instance_extensions` → `enable_debug_utils`,完全独立的另一套 | 运行期行为 |

`FeatureDependency` 是**纯数据,表达不了第 2 类**。所以今天 debug_utils 的"扩展名"和"messenger 创建"
强行分在两个机制、两个文件里。

**你的 `register_feature_debug_utils(manifest)` 把两者合并成一次调用** —— 模块在一处同时声明
"我要这个扩展"和"建好后帮我创建这个资源"。这是真实价值:消除割裂,模块自包含。

> 所以定位要准:manifest **不是替代** `FeatureDependency`,是**包住它 + 补上回调那一半**。

---

## 2. 守住静态机制:分层,不二选一

当前最漂亮的是 `k_feature<&vk::PhysicalDeviceVulkan12Features::timelineSemaphore>` —— 成员指针
NTTP 在编译期生成 enable/test,模块永不手写 lambda;`k_module_dependency` 是 constexpr,放 .rodata,
编译期可审计。

若把 manifest 做成"纯运行期可变、什么都 push 进 vector",就把这套 constexpr 冲掉了。

**精炼:`register_feature_xxx` 做两件事**,manifest 内部就是这两类的容器:

```cpp
// config/FeatureManifest.h
class FeatureManifest {
public:
    // (1) 静态:把模块已有的 constexpr k_module_dependency 指针塞进来(数据原样,不复制不丢 constexpr)
    void addDependency(const conf::FeatureDependency * dep) { m_dependencies.push_back(dep); }

    // (2) 运行期:注册阶段化回调(见 §3)
    void addInstanceResource(InstanceResourceFn fn) { m_instance_resources.push_back(std::move(fn)); }
    // ... 其他 phase

    std::span<const conf::FeatureDependency * const> dependencies() const { return m_dependencies; }
    std::span<const InstanceResourceFn> instanceResources() const { return m_instance_resources; }
private:
    std::vector<const conf::FeatureDependency *> m_dependencies;
    std::vector<InstanceResourceFn>              m_instance_resources;
    // std::vector<DeviceResourceFn>             m_device_resources;  // §3
};
```

```cpp
// surface/feature_dependencies.h —— 静态部分纯数据,照旧 constexpr
inline constexpr conf::FeatureDependency k_module_dependency { ... };  // 不动

// 模块的 register 函数 = 引用静态数据 + 注册回调
inline void register_feature_surface(FeatureManifest & manifest) {
    manifest.addDependency(&k_module_dependency);          // (1) 静态依赖原样
    // surface 无 after-instance 资源;swapchain 创建是用户行为,不在这
}

// debug_utils 没有 feature struct,只有扩展 + 回调
inline void register_feature_debug_utils(FeatureManifest & manifest) {
    manifest.addDependency(&k_debug_utils_dependency);     // 仅 VK_EXT_debug_utils 扩展名
    manifest.addInstanceResource([](vk::Instance inst) -> ResourceLease {  // (2) 回调
        return enable_debug_utils(inst);                   // 复用现有实现
    });
}
```

> `k_feature<&member>` / `FeatureDependency` / constexpr **全部保留**。manifest 只是在它们之上加一层
> "注册 + 回调"。两全。

---

## 3. 坑一:回调有"阶段",不能扁平装

你说的 "Context 才调用资源创建回调" —— 但回调不止一个时机:

| phase | 时机 | 例子 | 产物 owner |
|---|---|---|---|
| after-instance | instance 建好后 | debug messenger | Context |
| after-device | 某个 device 建好后 | 需要 device 的扩展资源 | 对应 DeviceContext |

如果 manifest 把所有回调塞一个 `vector<callback>`,create 流程无法知道哪个该在 instance 后调、
哪个该在 device 后调,且 device 回调要绑到**哪个** DeviceContext(多设备时)。

**精炼:回调按 phase 分桶**,每个 create_* / Context 阶段只取自己那桶:

```cpp
using InstanceResourceFn = std::function<ResourceLease(vk::Instance)>;
using DeviceResourceFn   = std::function<ResourceLease(vk::Device, vk::PhysicalDevice)>;
// manifest 内分别 vector;Context 在 instance 建好后跑 instanceResources,
// 在每个 device 建好后跑 deviceResources,把 lease 存进对应的 Context / DeviceContext
```

> 静态数据(扩展/feature)被 create_instance / select / create_device 在创建**期间**读;
> 回调被 Context 在创建**之后**调。两类的消费时机不同,phase 标签把它显式化。

---

## 4. 坑二:manifest 不是单数 —— 实例级 vs 每角色设备级

我们已确定 eMain / eCompute 可以有**不同的** feature 依赖(eMain 要 surface+sync,eCompute 只要
sync)。但 instance 只有一个。所以:

- **实例扩展** = 所有角色依赖的**并集**(一个 instance)。
- **设备 feature/扩展** = **每个角色各自**的集合。

你描述的「创建一个 manifest,调所有 register」是单数 manifest —— 这只在"所有设备角色用同一套依赖"
时成立。一旦 per-role 不同,就需要:

**方案 A(推荐):一个实例 manifest + 每角色一个设备 manifest。**
```cpp
FeatureManifest main_device_manifest;      register_feature_surface(main_device_manifest);
                                           register_feature_sync(main_device_manifest);
FeatureManifest compute_device_manifest;   register_feature_sync(compute_device_manifest);
// 实例扩展:Context 把所有角色 manifest 的 dependencies 的 instance_extensions 求并集
```
即 `DeviceContextCreateInfo` 里那个 `FeatureDependencyList` 升级成一个 `FeatureManifest`。
per-role setter(`setMainDeviceContextCreateInfo`)塞的就是该角色的 manifest。
instance 创建时 Context 遍历所有角色 manifest 取实例扩展并集(对应分层文档 §2.2 的来源合并)。

**方案 B**:单 manifest + 每条依赖标 scope(instance-only / device-only / both)。更省对象但
per-role 设备差异表达不了(单 manifest 无法让 eCompute 不要 surface)。→ 不满足你的双 GPU 场景,不推荐。

> 所以 manifest 的正确粒度是 **per-DeviceRole**(设备级),外加 Context 从它们派生出的实例级并集。
> 不是全局单数。

---

## 5. 精炼后的完整调用流程

```
// 用户侧:include 需要的 feature_deps,按角色建 manifest
#include "vk_core/surface/feature_dependencies.h"
#include "vk_core/sync/feature_dependencies.h"
#include "vk_core/details/instance_extensions/debug_utils.h"   // register_feature_debug_utils

FeatureManifest main_mf;
register_feature_debug_utils(main_mf);   // 实例回调 + 扩展
register_feature_surface(main_mf);       // 实例扩展 + 设备扩展(swapchain)
register_feature_sync(main_mf);          // 设备 feature(timeline)

ContextCreateInfo info;
info.setApplicationInfo(...)
    .setMainDeviceContextManifest(std::move(main_mf))           // per-role
    .setComputeDeviceContextManifest(std::move(compute_mf));
context.create(info);
```

```
Context::create:
  ├─ 遍历所有角色 manifest → 实例扩展并集 → details::create_instance(InstanceCreateInfo)
  ├─ 跑 main_mf.instanceResources() 回调 → leases 存进 Context        ← after-instance phase
  └─ 每个 DeviceRole:
       ├─ details::select_physical_device(instance, 该角色 manifest.dependencies())
       ├─ 去重物理设备(合并各角色 manifest 的 dependencies)
       ├─ details::create_device(pd, 该设备的 enabled deps + queue families)
       └─ 跑该角色 manifest.deviceResources(device, pd) 回调 → leases 存进 DeviceContext  ← after-device phase
```

create_* 三个原语**只读 manifest 的静态 `dependencies()` 那一片**(扩展/feature),仍不认识
DeviceRole/QueueRole(满足分层文档的边界)。回调由 Context 在创建后按 phase 调。

---

## 6. 权衡

| | 现状(纯 constexpr FeatureDependency + 独立 enabler 表) | FeatureManifest 统一注册面 |
|---|---|---|
| 一个 feature 的扩展 + 回调 | 散在两个机制/文件 | 一个 `register_feature_xxx` 合并声明 ✅ |
| 静态 feature bits constexpr | ✅ | ✅(分层保留,§2) |
| ADR-0001 编译期可审计 | ✅ 调用点列出 manifest 指针 | ✅ 调用点列出 register 调用 |
| debug_utils 扩展名归属 | 用户手填 main.cpp ❌ | 模块 register 自带 ✅ |
| 回调时机/owner | enabler 表只有 instance 级,无 device 级 | phase 标签显式表达 ✅ |
| 复杂度 | 两套简单机制 | 一个稍复杂的 manifest,但消除割裂 |
| 运行期开销 | 静态零成本 + 表遍历 | manifest 运行期构建(bootstrap 一次,可忽略) |

**建议:采用,但按 §2 分层(静态数据仍 constexpr)、§3 phase 分桶、§4 per-role 粒度。** 朴素的
"一个扁平 manifest 装所有东西、全局单数"会同时踩 §3 §4 两个坑,且冲掉 §2 的 constexpr 机制 ——
那样反而不如现状。精炼版才是净胜。

---

## 7. 一句话总结

更优雅,因为它把"扩展声明"和"资源创建回调"合并进模块自己的 `register_feature_xxx`,消除今天的
割裂。但 manifest 是 `FeatureDependency` 的**容器 + 回调层**,不是替代品:静态 bits 仍 constexpr;
回调按 after-instance / after-device 分桶;manifest 粒度是 per-DeviceRole,实例扩展是 Context 从中
派生的并集。create_* 原语只读静态那片,仍不认识 role 枚举。

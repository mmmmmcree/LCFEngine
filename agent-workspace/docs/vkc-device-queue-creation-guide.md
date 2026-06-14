# vkc Device & Queue 创建实现指南

照着这份文档实现 `DeviceContext` / `QueueContext` 的创建 —— 它是 `Context::create`
里的 device-fold 阶段。配套文档 `vkc-context-initialization.md` 讲的是 instance 阶段。
这里的代码片段是**指引用的骨架**,不是可直接粘贴的成品,错误处理和边界情况你自己补全。

参考锚点:`RenderEngine/src/Vulkan/VulkanContext.cpp`(被替换的旧形态)、
`libs/vk_core/src/context/Context.cpp`(已完成的 instance 阶段,以及要照搬的
`_maythrow` / `noexcept` 边界模式)。

一次性文档,用完即弃。

---

## 0. 心智模型

```
Context (instance, debug, instance dispatch)
  └── 拥有 std::vector<DeviceContext>          // 每个被选中的「不同物理设备」一个
        └── 拥有 std::vector<QueueContext>     // 创建出的每个队列(按族封顶的全拓扑)
              └── 拥有 vk::Queue + TimelineSemaphore + 提交计数器

Context::m_device_table : array<DeviceContext*, enum_count_v<DeviceRole>>   // 角色 → 设备(可别名)
DeviceContext::m_queue_table : array<QueueContext*, enum_count_v<QueueRole>> // 角色 → 队列(可别名)
```

两张索引表(`*_table`)把**角色**映射到扁平的 ownership vector 上。表里存的是指向
vector 元素的**指针**,所以稀缺硬件上多个角色可以别名同一个对象。Ownership 在 vector,
查找用 array-by-enum。没有 `eCount` 哨兵 —— 用 `enum_count_v` 给数组定长。

设计文档里的核心不变量:**整个流程不出现 surface / window / present 查询**。
present-family 的解析推迟到 `Swapchain::create`。

---

## 1. `QueueRole` 即理想队列数的唯一真相来源

这是你提出的核心点。每个 `QueueRole` 理想上都想要**自己独占**一个队列。把角色按它需要
的队列能力("flavor")归类,某个 flavor 下的角色个数,就是该族的理想队列数。加一个角色,
对应 flavor 的理想数自动 +1,不需要手维护 `{graphics:2, compute:2, transfer:1}` 这种表。

```cpp
enum class QueueRole : uint8_t {
    eMainGraphics = 0,   // → graphics flavor
    eSubGraphics,        // → graphics flavor   (第 2 个 graphics 队列)
    eCompute,            // → compute  flavor
    eTransfer,           // → transfer flavor
};
```

于是理想数推导为:graphics = 2(`eMainGraphics` + `eSubGraphics`)、compute = 1、
transfer = 1。

### 1.1 角色 → flavor 映射(唯一需要手写的小表)

```cpp
// context/enums.h(或一个小的 details 头)
constexpr vk::QueueFlagBits queue_role_flavor(enums::QueueRole role) noexcept
{
    using enums::QueueRole;
    switch (role) {
        case QueueRole::eMainGraphics:
        case QueueRole::eSubGraphics: return vk::QueueFlagBits::eGraphics;
        case QueueRole::eCompute:     return vk::QueueFlagBits::eCompute;
        case QueueRole::eTransfer:    return vk::QueueFlagBits::eTransfer;
    }
    return vk::QueueFlagBits::eGraphics; // 不可达;消除告警
}
```

### 1.2 每个 flavor 的理想数,从枚举推导

```cpp
constexpr uint32_t ideal_queue_count(vk::QueueFlagBits flavor) noexcept
{
    uint32_t count = 0;
    for (auto role : magic_enum::enum_values<enums::QueueRole>()) {
        if (queue_role_flavor(role) == flavor) { ++count; }
    }
    return count;
}
```

`magic_enum::enum_values` 本项目已经在用(见 `enum_count.h`)。这是 `constexpr`,计数在
编译期定。你要建族的三个 flavor 是 graphics / compute / transfer;对每个查
`ideal_queue_count(flavor)` 即可。

---

## 2. `DeviceContextCreateInfo` —— 每个角色的设备选择偏好

放在 `context/DeviceContext.h`。**只**通过 `ContextCreateInfo` 的 per-role setter 设置
(见 §3),用户不会单独构造它。它携带 fold 为某个 `DeviceRole` 挑选并配置设备所需的一切。

```cpp
// context/DeviceContext.h
class DeviceContextCreateInfo
{
    using Self = DeviceContextCreateInfo;
    using FeatureDependencyList = std::vector<const conf::FeatureDependency *>;
    using Scorer = std::function<int(vk::PhysicalDevice)>;
public:
    // 硬过滤不在这里 —— 硬过滤就是下面的 feature dependencies。这两项只影响「幸存设备里谁胜出」。
    Self & setPreferredDeviceType(vk::PhysicalDeviceType type) noexcept       // eDiscreteGpu / eIntegratedGpu
    { m_preferred_type = type; return *this; }
    Self & setScorer(Scorer scorer) noexcept                                  // 仅作参考排序
    { m_scorer = std::move(scorer); return *this; }
    Self & addFeatureDependency(const conf::FeatureDependency * dependency)   // 该角色要用的模块
    { m_feature_dependencies.push_back(dependency); return *this; }
    Self & addFeatureDependencies(convertible_range_of_c<const conf::FeatureDependency *> auto && deps)
    { m_feature_dependencies.append_range(deps); return *this; }

    // getXxx 访问器 ...
    bool isActive() const noexcept
    { return not m_feature_dependencies.empty() or m_preferred_type.has_value() or bool(m_scorer); }
//- properties
    std::optional<vk::PhysicalDeviceType> m_preferred_type;
    Scorer m_scorer;
    FeatureDependencyList m_feature_dependencies;
};
```

`isActive()` 让 fold 跳过用户从没配过的角色(比如用户只要 `eMain`,`eCompute` 留空 ——
那个角色就别名到 `eMain`,见 §6.3)。

`convertible_range_of_c` 和 `ContextCreateInfo::addRequiredInstanceExtensions` 用的是同一个
concept,直接复用。

---

## 3. `ContextCreateInfo` —— per-role setter

`ContextCreateInfo` 已经有 application info、实例扩展/层、capabilities flags。现在加一个
按 `DeviceRole` 索引的 `DeviceContextCreateInfo` 数组,以及一一对应的 setter。

```cpp
// context/Context.h, ContextCreateInfo 内
Self & setMainDeviceContextCreateInfo(const DeviceContextCreateInfo & info) noexcept
{ m_device_create_infos[std::to_underlying(enums::DeviceRole::eMain)] = info; return *this; }
Self & setComputeDeviceContextCreateInfo(const DeviceContextCreateInfo & info) noexcept
{ m_device_create_infos[std::to_underlying(enums::DeviceRole::eCompute)] = info; return *this; }

const auto & getDeviceContextCreateInfo(enums::DeviceRole role) const noexcept
{ return m_device_create_infos[std::to_underlying(role)]; }

//- properties
std::array<DeviceContextCreateInfo, enum_count_v<enums::DeviceRole>> m_device_create_infos;
```

> 加 `DeviceRole` 时,数组长度自动跟着 `enum_count_v` 变;你只需补一个对应的 setter。
> setter 数量和枚举对齐是设计的明确约定(你的要求),不要做成单个泛型 `setDeviceContextCreateInfo(role, info)`
> —— 显式命名的 setter 才是审计面。

### 3.1 实例扩展的来源合并

instance 阶段现在多一个扩展来源。最终启用的实例扩展集 = 下面三者的并集,再 ∩ 可用:

1. `m_required_instance_extensions` —— 用户显式加的(debug utils 等)。
2. **每个 active 角色**的 feature dependencies 里的 `instance_extensions`(展开 optional)。
   surface 模块的 `VK_KHR_surface` + 平台扩展就是从这里来的。
3. (可选)`ContextCapabilitiesFlags::eSurface` 这类高层开关,如果你想用它再补一层。

也就是说 `create_instance_maythrow` 里组装扩展时,要先把所有角色 create-info 的依赖
flatten 一遍(见 §4 的 `flatten_dependencies`),把它们的 `instance_extensions` 并进
wish-list,再和 `enumerateInstanceExtensionProperties()` 求交。Layer 维持现状。

---

## 4. 依赖展开:`flatten_dependencies`

`FeatureDependency` 通过 `optional` 字段挂着子依赖(如 surface 的
`k_swapchain_maintenance_dependency`)。fold 之前要把一个角色的依赖列表递归展开 + dedup,
得到一个扁平的指针集合。这是 `DeviceContext.cpp` 底部匿名命名空间里的 helper。

```cpp
// 递归收集 deps 及其 optional 子依赖;用 set 去重(按指针)
void collect_dependencies(const conf::FeatureDependency * dep,
                          std::unordered_set<const conf::FeatureDependency *> & out)
{
    if (not dep or not out.insert(dep).second) { return; }  // 已收集过则停
    for (const auto * child : dep->optional) { collect_dependencies(child, out); }
}

std::vector<const conf::FeatureDependency *>
flatten_dependencies(std::span<const conf::FeatureDependency * const> deps)
{
    std::unordered_set<const conf::FeatureDependency *> seen;
    for (const auto * dep : deps) { collect_dependencies(dep, seen); }
    return seen | stdr::to<std::vector>();
}
```

注意:`optional` 子依赖也被收进来了。它们参与「能不能启用」的探测,但**不参与硬过滤**
(硬过滤只看顶层 required 依赖,见 §5.2)。展开后你会在评估时区分这两类。

> 实务建议:把「required 顶层依赖」和「展开后的全部依赖(含 optional)」分别保留。
> 硬过滤用前者,能力快照/打分用后者。

---

## 5. 单设备评估:能不能用 + 打多少分

对**每个**物理设备、针对**某个角色的依赖集合**,做一次评估。返回「是否通过硬过滤」+
「分数」+「实际能启用的依赖/扩展/特性」。

### 5.1 一次性查询 FeatureChain

把这个角色 flatten 后所有依赖引用到的 feature struct 都 `request` 进一个 `FeatureChain`,
然后 `queryFrom(physical_device)` 一次拿到该设备实际支持情况。

```cpp
conf::FeatureChain build_query_chain(std::span<const conf::FeatureDependency * const> flat_deps)
{
    conf::FeatureChain chain;
    for (const auto * dep : flat_deps) {
        for (const auto & feature_bit : dep->features) { feature_bit.enable(chain); }  // 把对应 struct 挂进链
    }
    return chain;
}
// 用法:auto chain = build_query_chain(flat); chain.queryFrom(pd);
```

> `k_feature<...>.enable(chain)` 会 `request<Struct>()` 把 struct 挂进 pNext 链并置位。
> 用作 query 时置不置位无所谓,关键是把 struct **挂进链**,这样 `getFeatures2` 才会填它。
> query 完用 `feature_bit.test(chain)` 读回设备是否真支持这一位。

### 5.2 一个依赖「在这台设备上是否被满足」

```cpp
bool is_dependency_supported(const conf::FeatureDependency & dep,
                             const conf::FeatureChain & queried,
                             const std::unordered_set<std::string> & available_device_extensions,
                             uint32_t api_version)
{
    // core_since:该 API 版本已把能力并入核心,则扩展名可忽略
    bool needs_extensions = (dep.core_since == 0) or (api_version < dep.core_since);
    if (needs_extensions) {
        for (const char * ext : dep.device_extensions) {
            if (not available_device_extensions.contains(ext)) { return false; }
        }
    }
    for (const auto & feature_bit : dep.features) {
        if (not feature_bit.test(queried)) { return false; }
    }
    return true;
}
```

`available_device_extensions` 来自 `physical_device.enumerateDeviceExtensionProperties()`
转成的 set。

### 5.3 硬过滤 + 打分

```cpp
struct DeviceEvaluation {
    bool   passed = false;   // 所有 required 顶层依赖都满足
    int    score  = 0;       // (支持的 optional 数, scorer 分) 的综合
    // 实际支持的依赖集合(required + 满足的 optional),供 §6 建 enable 链用
    std::vector<const conf::FeatureDependency *> supported;
};

DeviceEvaluation evaluate_device(vk::PhysicalDevice pd,
                                 const DeviceContextCreateInfo & info,
                                 uint32_t api_version)
{
    auto required = info.m_feature_dependencies;                  // 顶层 = required
    auto flat     = flatten_dependencies(required);              // 含 optional
    auto chain    = build_query_chain(flat); chain.queryFrom(pd);
    auto exts     = /* enumerateDeviceExtensionProperties → set<string> */;

    DeviceEvaluation eval;
    // 硬过滤:每个 required 顶层依赖都必须满足
    for (const auto * dep : required) {
        if (not is_dependency_supported(*dep, chain, exts, api_version)) { return eval; } // passed=false
    }
    eval.passed = true;
    eval.supported = required;
    // optional:支持就纳入,并加分(仅 tie-break 权重)
    for (const auto * dep : flat) {
        if (stdr::find(required, dep) != required.end()) { continue; } // 跳过已是 required 的
        if (is_dependency_supported(*dep, chain, exts, api_version)) {
            eval.supported.push_back(dep);
            eval.score += 1;                                     // 支持的 optional 越多越好
        }
    }
    if (info.m_scorer) { eval.score += info.m_scorer(pd); }      // 用户 scorer:仅参考
    return eval;
}
```

注意顺序:**硬过滤先于 score**。一个 scorer 给 100000 分但缺 required 扩展的设备,
直接被 `passed=false` 淘汰 —— 这正是你强调的「score 仅参考,实际要看是否支持所有依赖」。

---

## 6. fold 编排:从角色到 DeviceContext

这是 `DeviceContext.cpp` 的核心,由 `Context::create` 在 instance 建好后调用。按
cpp-style 的 .cpp 布局:顶部匿名 ns 放 helper 声明,中部 named ns 放对外定义,底部匿名 ns
放 helper 定义。

整体次序:**每个角色选出最优设备 → 按物理设备去重 → 每个去重后的设备建一次 vk::Device →
建 QueueContext + QueueRole 表 → emplace 进 Context + 填 DeviceRole 表(别名)**。

### 6.1 每个角色选最优设备(含类型偏好)

```cpp
struct RoleSelection {
    vk::PhysicalDevice physical_device;
    std::vector<const conf::FeatureDependency *> supported;  // 来自 §5 的 eval.supported
    bool valid = false;
};

RoleSelection select_device_for_role(vk::Instance instance,
                                     const DeviceContextCreateInfo & info,
                                     uint32_t api_version)
{
    if (not info.isActive()) { return {}; }                  // 未配置的角色:留给 §6.3 别名

    struct Candidate { vk::PhysicalDevice pd; DeviceEvaluation eval; vk::PhysicalDeviceType type; };
    std::vector<Candidate> candidates;
    for (auto pd : instance.enumeratePhysicalDevices()) {
        auto eval = evaluate_device(pd, info, api_version);
        if (not eval.passed) { continue; }                   // 硬过滤
        candidates.push_back({pd, std::move(eval), pd.getProperties().deviceType});
    }
    if (candidates.empty()) { return {}; }                   // 该角色无可用设备(上层据此报错)

    // 类型偏好:若设了 preferred type 且存在该类型幸存者,只在该类型里选;否则全体里选。
    auto type_matches = [&](const Candidate & c) {
        return info.m_preferred_type and c.type == *info.m_preferred_type;
    };
    bool has_preferred = info.m_preferred_type and stdr::any_of(candidates, type_matches);

    auto best = stdr::max_element(candidates, {}, [&](const Candidate & c) {
        // 排序键:先「是否命中偏好类型」,再 score。tuple 比较天然字典序。
        int prefer = (has_preferred and type_matches(c)) ? 1 : 0;
        return std::pair{prefer, c.eval.score};
    });
    return { best->pd, std::move(best->eval.supported), true };
}
```

> 类型偏好是「软」的:设了独显但机器只有集显,不会失败,而是回退到集显(只要它通过硬过滤)。
> 若你想要「偏好类型不满足就报错」的硬语义,把 `has_preferred` 那段改成:无匹配则
> `return {}`。默认建议软回退。
> 你这台双 GPU 机器:eMain 设 `eDiscreteGpu`、eCompute 设 `eIntegratedGpu`,就能分别选到
> 独显和集显,得到两个不同 DeviceContext 做测试。

### 6.2 按物理设备去重

多个角色可能选到同一个 `vk::PhysicalDevice`(单 GPU,或两个角色都偏好独显)。同一物理设备
只建一个 `vk::Device` / `DeviceContext`,并把这些角色需要的依赖**并集**起来一起 enable。

```cpp
// 角色 → 选择结果
std::array<RoleSelection, enum_count_v<enums::DeviceRole>> selections;
for (auto role : magic_enum::enum_values<enums::DeviceRole>()) {
    selections[std::to_underlying(role)] =
        select_device_for_role(instance, create_info.getDeviceContextCreateInfo(role), api_version);
}

// 去重:VkPhysicalDevice 句柄 → 合并后的依赖并集
struct DeviceGroup {
    vk::PhysicalDevice pd;
    std::unordered_set<const conf::FeatureDependency *> deps;  // 该设备上所有角色 supported 的并集
};
std::vector<DeviceGroup> groups;
for (auto & sel : selections) {
    if (not sel.valid) { continue; }
    auto it = stdr::find(groups, sel.physical_device, &DeviceGroup::pd);
    if (it == groups.end()) { groups.push_back({sel.physical_device, {}}); it = std::prev(groups.end()); }
    it->deps.insert_range(sel.supported);
}
```

> `vk::PhysicalDevice` 是个句柄(本质 `VkPhysicalDevice` 指针),可直接比较相等 —— 同一物理
> 设备在同一 instance 下枚举出来句柄相同,所以可用它做 dedup 键。

### 6.3 创建每个去重设备的 DeviceContext

对每个 `DeviceGroup` 调 `DeviceContext::create`(见 §7),拿到的对象 `emplace_back` 进
`Context::m_device_contexts`。然后填 `DeviceRole` 表:每个 valid 的角色,指针指向它所属
group 创建出的那个 DeviceContext。

```cpp
// groups 顺序建 DeviceContext;记录 pd → DeviceContext* 便于回填角色表
std::unordered_map<VkPhysicalDevice, DeviceContext *> pd_to_ctx;
for (auto & group : groups) {
    DeviceContext ctx;
    if (auto ec = ctx.create(group.pd, group.deps, /* queue/特性参数 */)) { return ec; }
    m_device_contexts.push_back(std::move(ctx));
    pd_to_ctx[group.pd] = &m_device_contexts.back();
}
// 角色表(别名安全):未配置的角色回退到 eMain
for (auto role : magic_enum::enum_values<enums::DeviceRole>()) {
    auto & sel = selections[std::to_underlying(role)];
    DeviceContext * target = sel.valid ? pd_to_ctx.at(sel.physical_device)
                                       : m_device_table[std::to_underlying(enums::DeviceRole::eMain)];
    m_device_table[std::to_underlying(role)] = target;
}
```

> ⚠️ 陷阱:`m_device_contexts` 是 `std::vector`,`push_back` 可能 reallocate,使之前存的
> `&back()` 指针失效。两个安全做法:(a) 建完所有 DeviceContext 后再统一回填指针表
> (按句柄查,不存裸指针);或 (b) 先 `m_device_contexts.reserve(groups.size())`。
> 推荐 (b) + 建完再回填,最稳。`DeviceRole::eMain` 必须保证 valid(否则别名无目标)——
> 约定:eMain 是必配角色,fold 开头校验它 `isActive()`,否则返回错误。

---

## 7. `DeviceContext::create` —— 队列族解析 + 建 device + 建队列

签名建议:
```cpp
std::error_code DeviceContext::create(
    vk::PhysicalDevice physical_device,
    std::span<const conf::FeatureDependency * const> supported_dependencies,
    uint32_t api_version) noexcept;
```
内部同样用 `_maythrow` 写核心、`noexcept` 边界 catch `vk::SystemError`,与 `Context.cpp` 一致。

### 7.1 解析队列族(无 surface 依赖)

旧 `findQueueFamilies` 用 `getSurfaceSupportKHR` 把 graphics 族绑死到 surface —— **整段删掉**。
我们只按 queue flags 选族:对每个 flavor 找一个「有该能力、且尽量不带其他能力」的专用族。

```cpp
// desired:必须有的能力;undesired:尽量没有(优先专用族,如纯 transfer DMA 队列)
std::optional<uint32_t> find_queue_family(
    std::span<const vk::QueueFamilyProperties> families,
    vk::QueueFlags desired, vk::QueueFlags undesired)
{
    std::optional<uint32_t> fallback;
    for (uint32_t i = 0; i < families.size(); ++i) {
        if (not (families[i].queueFlags & desired)) { continue; }
        if (not (families[i].queueFlags & undesired)) { return i; }  // 命中专用族,直接用
        if (not fallback) { fallback = i; }                          // 退而求其次
    }
    return fallback;
}
```

三个 flavor 的查找参数(和旧代码一致):
```cpp
// graphics:任意带 graphics 的族
// transfer:带 transfer、尽量不带 compute(偏好独立 DMA 队列)
// compute :带 compute、尽量不带 transfer
auto graphics_family = find_queue_family(families, eGraphics, {});
auto transfer_family = find_queue_family(families, eTransfer, eCompute);
auto compute_family  = find_queue_family(families, eCompute,  eTransfer);
```

> 注意:graphics 族通常也带 compute+transfer。在很多设备上 transfer/compute 的「专用族」
> 不存在,`find_queue_family` 会回退到 graphics 族 —— 这没问题,collapse ladder(§7.4)会
> 处理多个 role 落到同一队列的情况。

### 7.2 计算每族的队列数(理想数封顶)

理想数来自 §1.2 的 `ideal_queue_count(flavor)`,再对 family 实际 `queueCount` 取 min。
**多个 flavor 落到同一族时,该族的队列数是各 flavor 理想数之和**(再封顶),这样仍尽量给每个
role 留独立队列。

```cpp
// family index → 该族要创建的队列数
std::unordered_map<uint32_t, uint32_t> family_queue_counts;
auto request_queues = [&](std::optional<uint32_t> fam, vk::QueueFlagBits flavor) {
    if (not fam) { return; }
    family_queue_counts[*fam] += ideal_queue_count(flavor);   // 同族累加各 flavor 的理想数
};
request_queues(graphics_family, eGraphics);   // +2
request_queues(compute_family,  eCompute);    // +1
request_queues(transfer_family, eTransfer);   // +1
// 封顶到硬件可用
for (auto & [fam, count] : family_queue_counts) {
    count = std::min(count, families[fam].queueCount);
}
```

### 7.3 建 device(联动 FeatureChain + 扩展并集)

```cpp
// (a) enable 链:把 supported_dependencies 的所有 feature 置位
conf::FeatureChain enable_chain;
for (const auto * dep : supported_dependencies) {
    for (const auto & feature_bit : dep->features) { feature_bit.enable(enable_chain); }
}
// (b) 设备扩展并集(去重),core_since 命中则跳过
std::unordered_set<std::string> device_extensions;
for (const auto * dep : supported_dependencies) {
    bool needs = (dep->core_since == 0) or (api_version < dep->core_since);
    if (needs) { for (const char * e : dep->device_extensions) { device_extensions.insert(e); } }
}
auto ext_cstrs = device_extensions | stdv::transform(&std::string::c_str) | stdr::to<std::vector>();

// (c) queue create infos:priority 数组生命周期要活到 createDevice 调用
std::vector<std::vector<float>> priorities;       // 每族一组,持有到 create
std::vector<vk::DeviceQueueCreateInfo> queue_infos;
for (auto & [fam, count] : family_queue_counts) {
    priorities.emplace_back(count, 1.0f);          // 简单起见全 1.0;要分级可按 i 递减
    queue_infos.push_back(vk::DeviceQueueCreateInfo{}.setQueueFamilyIndex(fam).setQueuePriorities(priorities.back()));
}

// (d) pNext = enable 链的 root;createDeviceUnique
vk::DeviceCreateInfo device_info;
device_info.setQueueCreateInfos(queue_infos)
           .setPEnabledExtensionNames(ext_cstrs)
           .setPNext(&enable_chain.root());        // FeatureChain 已把所有 struct 串进 pNext
m_device = physical_device.createDeviceUnique(device_info);
m_physical_device = physical_device;

details::initialize_device(this->getDevice());     // 见下方多设备注意
```

> ⚠️ **enable_chain 的生命周期**:`createDeviceUnique` 读 `pNext` 链是在调用那一刻,所以
> `enable_chain` 必须活到该调用结束。它是栈上局部变量、在同一函数体内,天然满足。但**不要**
> 把 `enable_chain` 移动/销毁到 create 之前。FeatureChain 用 node-based map 保证 pNext 地址
> 稳定(见 `FeatureDeclare.h`),所以链内指针在 chain 存活期间有效。
>
> ⚠️ **多设备与 dispatch**:`VULKAN_HPP_DEFAULT_DISPATCHER` 是全局单例。默认(device
> dispatch 关闭)走 instance-level dispatch,一个 dispatcher 覆盖该 instance 下所有 device,
> 多 GPU 没问题。**但若开了 `LCF_VKC_USE_DEVICE_DISPATCH`,`init(device)` 会让后一个 device
> 覆盖前一个的函数指针,与多 device 冲突。** 所以多 GPU 测试时保持该选项默认 OFF。建议在
> `initialize_device` 的注释里补这条约束。

### 7.4 建 QueueContext + QueueRole collapse ladder

先把每族创建出的队列实例化成 `QueueContext`(全拓扑 ownership),再用 collapse ladder 把
`QueueRole` 映射上去。

```cpp
// (a) ownership:每族 count 个队列,逐个建 QueueContext
//     记录 family → 该族 QueueContext 在 m_queue_contexts 里的下标区间,供 ladder 取用
m_queue_contexts.reserve(/* 总队列数 */);
std::unordered_map<uint32_t, std::vector<QueueContext*>> family_queues;
for (auto & [fam, count] : family_queue_counts) {
    for (uint32_t i = 0; i < count; ++i) {
        QueueContext qc;
        qc.create(this->getDevice(), fam, i);      // 取 vk::Queue + 建 timeline
        m_queue_contexts.push_back(std::move(qc));
        family_queues[fam].push_back(&m_queue_contexts.back()); // 注意 reserve 防失效
    }
}
```

collapse ladder:每个 `QueueRole` 想要它 flavor 对应族里**还没被占用**的下一个队列;占用完了
就退到「同 flavor 的主队列」,再退到 `eMainGraphics`。因为 `QueueContext` 是唯一提交漏斗,
多个 role 别名同一队列是安全的。

```cpp
// (b) QueueRole 表:按枚举顺序分配,同族队列依次发放,不够则别名
//     flavor → 该 flavor 落在哪个 family(graphics/compute/transfer 解析结果)
std::unordered_map<vk::QueueFlagBits, std::optional<uint32_t>> flavor_family {
    {eGraphics, graphics_family}, {eCompute, compute_family}, {eTransfer, transfer_family},
};
std::unordered_map<uint32_t, uint32_t> next_index;   // family → 下一个可发放的队列序号
for (auto role : magic_enum::enum_values<enums::QueueRole>()) {
    auto flavor = queue_role_flavor(role);
    auto fam_opt = flavor_family[flavor];
    QueueContext * target = nullptr;
    if (fam_opt and family_queues.contains(*fam_opt)) {
        auto & queues = family_queues[*fam_opt];
        uint32_t idx = next_index[*fam_opt];
        if (idx < queues.size()) { target = queues[idx]; ++next_index[*fam_opt]; } // 拿独立队列
        else { target = queues.front(); }                                          // 该族发完,别名主队列
    } else {
        // 该 flavor 无专用族(如无独立 compute/transfer):退到 eMainGraphics 的队列
        target = m_queue_table[std::to_underlying(enums::QueueRole::eMainGraphics)];
    }
    m_queue_table[std::to_underlying(role)] = target;
}
```

> ladder 的退化次序:**专用族独立队列 → 同族已发队列(别名)→ eMainGraphics(别名)**。
> 这保证低端设备(单 graphics 族单队列)上 4 个 role 全部别名到同一个 QueueContext,而高端
> 设备(多族多队列)上尽量各占独立队列。`eMainGraphics` 必须最先分配且一定有队列(graphics
> 族总存在),作为所有退化的兜底。
>
> ⚠️ 同样注意 `m_queue_contexts` 的 reserve:ladder 存的是指针,vector 重分配会失效。先
> reserve 总队列数,或建完再回填。

---

## 8. `QueueContext` —— 补上 create 与访问器

当前只有两个裸成员。补成可用类型:

```cpp
// context/QueueContext.h
class QueueContext
{
    using Self = QueueContext;
public:
    QueueContext() = default;
    ~QueueContext() noexcept = default;
    QueueContext(const Self &) = delete;
    QueueContext(Self &&) = default;
    Self & operator=(const Self &) = delete;
    Self & operator=(Self &&) = default;

    std::error_code create(vk::Device device, uint32_t family_index, uint32_t queue_index)
    {
        m_family_index = family_index;
        m_queue = device.getQueue(family_index, queue_index);
        return m_timeline.create(device);          // TimelineSemaphore::create 已存在
    }
    const vk::Queue & getQueue() const noexcept { return m_queue; }
    uint32_t getFamilyIndex() const noexcept { return m_family_index; }
    TimelineSemaphore & timeline() noexcept { return m_timeline; }
    // 提交计数器 / submit 漏斗后续补(overview 说 QueueContext 是唯一提交入口)
private:
    vk::Queue m_queue;
    TimelineSemaphore m_timeline;
    uint32_t m_family_index = 0;
};
```

> `m_queue` 不带 owning(队列随 device 销毁),所以 rule-of-five 用默认 move 即可。
> `getQueue()` 暂时对外暴露,后续按 overview「raw queue 永不暴露、只通过 submit 漏斗」的目标
> 收紧 —— 当前阶段先让它能跑,提交接口下一阶段做。

---

## 9. DeviceContext 的能力快照

device 建好后,把「实际启用了什么」冻结进 DeviceContext,作为运行期契约。各模块的
`create` 入口(`Swapchain::create` 等)查这个快照,缺失就返回 `error_code`。

```cpp
// context/DeviceContext.h, 成员
conf::FeatureChain m_capabilities;                 // 即 §7.3 的 enable_chain,create 后 move 进来
std::unordered_set<std::string> m_enabled_device_extensions;

// 查询接口
bool isExtensionEnabled(std::string_view ext) const noexcept
{ return m_enabled_device_extensions.contains(std::string(ext)); }

bool supports(const conf::FeatureDependency & dep, uint32_t api_version) const
{
    bool needs = (dep.core_since == 0) or (api_version < dep.core_since);
    if (needs) { for (const char * e : dep.device_extensions) { if (not isExtensionEnabled(e)) return false; } }
    for (const auto & fb : dep.features) { if (not fb.test(m_capabilities)) return false; }
    return true;
}
```

> 快照存的是 enable 链(我们置位的「想启用且设备支持」的集合),不是重新 query 的结果 ——
> 因为只有通过硬过滤的依赖才会进 `supported_dependencies`,enable 链里的位就代表「确实启用了」。

---

## 10. 接入 `Context::create`

`Context.cpp` 现有流程在 instance + debug + 实例扩展之后,加 device fold:

```cpp
std::error_code Context::create(const ContextCreateInfo & create_info) noexcept
{
    details::initialize_loader();
    auto expected_instance = create_instance(create_info);   // §3.1:实例扩展现在含 deps 来源
    if (not expected_instance) { return expected_instance.error(); }
    m_instance = std::move(*expected_instance);
    details::initialize_instance(this->getInstance());
    m_extension_resource_leases = details::enable_instance_extensions(this->getInstance());

    if (auto ec = this->createDevices(create_info)) { return ec; }   // ← 新增:§6 的 fold
    return {};
}
```

`createDevices` 是 `Context` 的私有成员函数(或放进匿名 ns 的自由函数,传 `Context&`),
封装 §6 的「每角色选设备 → 去重 → 建 DeviceContext → 填 DeviceRole 表」。同样
`_maythrow` 内核 + `noexcept` 边界。

---

## 11. 落地清单(建议顺序)

按依赖从底向上写,每步都能独立编译:

1. **`enums.h`** —— 加 `queue_role_flavor` + `ideal_queue_count`(§1)。纯 constexpr,无依赖。
2. **`QueueContext.h`** —— 补 `create` / 访问器(§8)。依赖 TimelineSemaphore(已存在)。
3. **`DeviceContext.h`** —— `DeviceContextCreateInfo`(§2)+ DeviceContext 的 `create` 声明
   + 快照成员(§9)。
4. **`Context.h`** —— `ContextCreateInfo` 加 per-role setter + 数组(§3);`Context` 加
   `createDevices` 声明。
5. **`src/context/DeviceContext.cpp`**(新建)—— flatten(§4)、evaluate(§5)、
   `DeviceContext::create`(§7)。文件布局按 cpp-style:顶部匿名 ns 声明、中部 named ns 定义、
   底部匿名 ns 定义。
6. **`Context.cpp`** —— `create_instance_maythrow` 合并 deps 实例扩展(§3.1);加
   `createDevices`(§6)接入 `Context::create`(§10)。
7. **`initialize_device` 注释** —— 补多设备 + device dispatch 的约束(§7.3)。

不用改 CMake(源文件 GLOB)。不用改 `examples/vkc_test/main.cpp`(`create(info)` 签名不变;
要测多 GPU 再加 per-role setter 调用)。

### 自测建议(你的双 GPU 机器)

```cpp
vkc::DeviceContextCreateInfo main_dev, compute_dev;
main_dev.setPreferredDeviceType(vk::PhysicalDeviceType::eDiscreteGpu)
        .addFeatureDependency(&sync::k_module_dependency)
        .addFeatureDependency(&surf::k_module_dependency);
compute_dev.setPreferredDeviceType(vk::PhysicalDeviceType::eIntegratedGpu)
           .addFeatureDependency(&sync::k_module_dependency);
context_info.setMainDeviceContextCreateInfo(main_dev)
            .setComputeDeviceContextCreateInfo(compute_dev);
```
预期:eMain 选独显、eCompute 选集显,`m_device_contexts.size() == 2`,两者 device handle
不同。若把两个都设成独显,则去重为 1 个 DeviceContext,DeviceRole 表两槽别名同一对象。


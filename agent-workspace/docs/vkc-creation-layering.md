# vkc 创建流程分层:details 原语 + Context 编排

评估「把创建流程拆成 `details::create_instance` / `select_physical_device` / `create_device`
三个通用原语,由 Context 在其上定义 GPU 模型」是否可行,并给出每个原语的契约。
一次性文档,用完即弃。给文档不改代码。

参考现状:`libs/vk_core/include/vk_core/details/{create_instance,create_device,select_physical_device}.h`
(已建骨架)、`libs/vk_core/src/details/create_instance.cpp`(已实现,`_maythrow`/`noexcept` 边界)。

---

## 0. 结论

**可行,且分层正确。** details 层 = 无 policy 的机械创建原语(吃 CreateInfo,吐 vk handle);
Context = 在其上编排 + 定义 GPU 模型(role 表、去重、能力快照、queue 拓扑)。details 不认识
DeviceRole/QueueRole,Context 不写 Vulkan 样板。这条边界健康。

但有三个张力必须在契约里解决,否则拆分会漏掉 vkc 的核心机制:

1. **select 与 create 之间要传递 FeatureChain 查询结果**,不能各查一遍(§3)。
2. **"选哪个设备"是 policy,"设备能不能用"是机械检验** —— 这两件事在 select 里要分开(§3)。
3. **queue 拓扑(QueueRole/collapse ladder)是 GPU 模型,不属于 details** —— create_device 只接收
   "建哪些 queue"的机械描述,不认识 role(§4)。

核心判断:**details 层的边界 = "不需要知道 vkc 发明的任何枚举(DeviceRole/QueueRole)就能完成"。**
凡是需要这些枚举的逻辑,都属于 Context。

---

## 1. 职责划分总表

| 关注点 | 层 | 理由 |
|---|---|---|
| instance 扩展/层 ∩ available、建 instance、初始化 dispatch | `details::create_instance` | 纯机械,无 policy |
| 枚举物理设备、对每个跑硬过滤、按偏好/scorer 排序、选出 | `details::select_physical_device` | 选择策略可通用,但 policy 来自传入的 SelectInfo |
| 建 vk::Device(feature chain pNext + 扩展 + queue infos) | `details::create_device` | 纯机械,接收已算好的参数 |
| **合并多角色依赖、去重物理设备** | Context | 需要 DeviceRole 概念 |
| **QueueRole / collapse ladder / 理想队列数** | Context | 需要 QueueRole 概念 |
| **DeviceRole 表 / QueueRole 表 / 能力快照** | Context | vkc GPU 模型本身 |
| **per-role DeviceContextCreateInfo → details CreateInfo 的翻译** | Context | 模型到原语的适配 |

一句话:**details 接收"要建什么"的机械描述,Context 负责"根据角色模型算出要建什么"。**

---

## 2. `create_instance` —— 已基本成形,两个修正

现状已实现,`_maythrow`/`noexcept` 边界正确。两处要改:

### 2.1 `requiresLayer` 查错了集合(既有 bug)

```cpp
bool requiresLayer(const std::string & layer_name) const noexcept
{ return m_required_instance_extensions.contains(layer_name); }   // ← 查的是 extensions
```
应查 `m_required_instance_layers`。否则 layer 过滤永远失效。

### 2.2 实例扩展还缺 feature-dependency 来源

当前 `InstanceCreateInfo` 只有用户显式加的扩展。但 surface 模块的 `VK_KHR_surface` + 平台扩展
来自 `surf::k_module_dependency.instance_extensions`。两个选择:

- **(A) Context 先把 deps 的实例扩展 flatten 出来,`addRequiredInstanceExtension` 进去** —— details
  保持纯净,不认识 FeatureDependency。**推荐**,符合分层。
- (B) `InstanceCreateInfo` 直接接收 `span<const FeatureDependency*>` —— details 沾上 conf 概念,不推荐。

走 (A):Context 在调 `create_instance` 前,把所有 active 角色的 deps 展开,实例扩展并进
`InstanceCreateInfo`。`create_instance` 永远只看到一个最终的扩展字符串集合。

---

## 3. `select_physical_device` —— 最关键的契约

这是三个原语里最容易设计错的。两个必须解决的问题:

### 3.1 硬过滤 vs 排序偏好要分开

"选哪个设备"是两件事的叠加:

- **硬过滤(机械、客观)**:这个设备支不支持要求的扩展 + feature bits + queue families。
  不支持就**没资格**,与分数无关。
- **排序偏好(policy、主观)**:在合格设备里,优先独显?用户 scorer 打分?

`PhysicalDeviceSelectInfo` 要同时携带这两类输入,且 select 内部**先过滤后排序**:

```cpp
class PhysicalDeviceSelectInfo {
    // —— 硬过滤输入(客观)——
    std::span<const conf::FeatureDependency * const> required_dependencies;  // 必须全支持
    uint32_t api_version = vk::ApiVersion13;        // 配合 core_since
    vk::QueueFlags required_queue_flags = {};        // 至少要有这些能力的族(eMain 需 graphics 等)
    // —— 排序偏好(policy)——
    std::optional<vk::PhysicalDeviceType> preferred_type;
    std::function<int(vk::PhysicalDevice)> scorer;   // 仅参考
    // —— 可选优化输入 ——
    std::span<const conf::FeatureDependency * const> optional_dependencies;  // 支持则加分
};
```

> required 与 preferred 混在一起是最常见的设计错误:scorer 给 100000 分但缺 required 扩展的设备
> 绝不能被选中。把它们放进不同字段,且文档化「filter 先于 sort」,就堵死了这个错误。

### 3.2 select 与 create 之间的 FeatureChain 复用(性能 + 正确性)

select 内部已经对每个设备建 FeatureChain、`queryFrom` 查了一次(才能做硬过滤)。create_device
建 device 时又要一个 enable 链。**如果 select 只返回 `vk::PhysicalDevice`,这个查询结果就丢了**,
create 阶段得重查一遍——浪费,且两次查询之间逻辑可能漂移。

所以 select 的返回值应携带「为什么选它 + 查到了什么」:

```cpp
struct PhysicalDeviceSelection {
    vk::PhysicalDevice physical_device;
    // 该设备实际满足的依赖(required + 支持的 optional)—— create 据此建 enable 链和扩展并集
    std::vector<const conf::FeatureDependency *> supported_dependencies;
    int score = 0;
};
std::expected<PhysicalDeviceSelection, std::error_code>
select_physical_device(vk::Instance, const PhysicalDeviceSelectInfo &) noexcept;
```

> 返回 `expected`:没有任何设备通过硬过滤是一个**可恢复错误**(上层据此报"无合适 GPU"),
> 不该返回裸 `vk::PhysicalDevice{}` 让调用方猜。当前骨架的 `noexcept` 返回裸句柄要改成 expected。

> ⚠️ supported_dependencies 是 select 和 create 之间的契约纽带。但注意:它**不**包含 queue 拓扑
> 信息——select 只验证"有没有满足 required_queue_flags 的族",不决定"建几个 queue"。后者是
> Context 的 QueueRole 模型(§4)。

---

## 4. `create_device` —— 纯机械,不认识 QueueRole

这是分层最容易破的地方。诱惑是让 create_device 直接接收 QueueRole 信息、内部跑 collapse ladder。
**不要。** collapse ladder / QueueRole / 理想队列数都是 vkc 发明的 GPU 模型,属于 Context。
create_device 只接收**已经算好的、用 vk 原生概念表达的** queue 描述:

```cpp
class DeviceCreateInfo {
    // Context 已翻译好的机械参数:
    std::span<const conf::FeatureDependency * const> enabled_dependencies;  // → enable 链 + 扩展并集
    uint32_t api_version = vk::ApiVersion13;
    // queue 描述:family index → 在该族建几个队列。Context 用 QueueRole 算出,这里只是 (index,count)
    std::span<const std::pair<uint32_t, uint32_t>> queue_families;          // {family_index, queue_count}
    // 或更明确:std::span<const vk::DeviceQueueCreateInfo>,但 priority 生命周期由调用方持有
};
```

create_device 内部(机械):
1. 对 `enabled_dependencies` 建 enable 链(`feature_bit.enable`),pNext = chain.root()。
2. 扩展并集(`core_since` 命中则跳过),转 cstr。
3. 用 `queue_families` 建 `vk::DeviceQueueCreateInfo`(priority 数组在函数内构造,活到 create)。
4. `createDeviceUnique` + `initialize_device`。
5. 返回 `vk::UniqueDevice`(以及可能需要的 enable 链快照,见下)。

> **谁拥有能力快照?** create_device 建了 enable 链,但快照(`FeatureChain` + enabled 扩展集)是
> DeviceContext 的成员。两个选择:(A) create_device 返回 `{UniqueDevice, FeatureChain, ext_set}`
> 让 Context 存进 DeviceContext;(B) Context 自己持有 enable 链,create_device 只收 `const&` 用、
> 不拥有。推荐 (A)——create_device 已经构造了链,顺手返回避免重复构造,且 details 不持有任何状态。

> **queue 拓扑的归属再强调**:`{family_index, queue_count}` 这个 span 是 **Context 用 QueueRole +
> collapse ladder 算出来的结果**。create_device 看到的只是"在 family 3 建 2 个、family 5 建 1 个"
> 这种纯 vk 描述。它永远不知道 eMainGraphics 是什么。建完 device 后,Context 再用
> `device.getQueue(family, i)` 取出队列、套上 QueueRole 表。

---

## 5. 编排:Context 把原语串起来

```
ContextCreateInfo (per-role DeviceContextCreateInfo)
  │
  ├─ Context: flatten 所有 active 角色的 deps,实例扩展并入 details::InstanceCreateInfo
  ├─ details::create_instance(InstanceCreateInfo)                     → vk::UniqueInstance
  │
  └─ 对每个 DeviceRole:
       ├─ Context: DeviceContextCreateInfo → details::PhysicalDeviceSelectInfo
       │            (required_dependencies, preferred_type, scorer, required_queue_flags)
       ├─ details::select_physical_device(instance, SelectInfo)       → PhysicalDeviceSelection
       │            (physical_device + supported_dependencies)
       │
       ├─ Context: 按 physical_device 去重多个角色,合并 supported_dependencies
       │
       └─ 对每个去重后的物理设备:
            ├─ Context: 解析 queue families + QueueRole 算理想数 → {family,count} 列表
            ├─ Context: 组装 details::DeviceCreateInfo (enabled_deps + queue_families)
            ├─ details::create_device(pd, DeviceCreateInfo)           → vk::UniqueDevice + 快照
            ├─ Context: 建 QueueContext(device.getQueue)、跑 collapse ladder 填 QueueRole 表
            └─ Context: emplace DeviceContext,填 DeviceRole 表(别名)
```

**details 出现在每个箭头的"动词"上,Context 出现在每个"翻译/决策"上。** 这就是分层成立的标志:
拿掉 Context,details 仍是一组可独立测试、可被其他工具复用的 Vulkan 创建原语;拿掉 details,
Context 仍完整表达 vkc 的 GPU 模型,只是要自己写样板。

---

## 6. 落地清单

1. **修 `create_instance.h`**:`requiresLayer` 查 `m_required_instance_layers`(§2.1)。
2. **`select_physical_device.h`**:`PhysicalDeviceSelectInfo` 补 required/preferred 两类字段;
   返回值改 `expected<PhysicalDeviceSelection, error_code>`(§3)。
3. **`create_device.h`**:`DeviceCreateInfo` 补 `enabled_dependencies` + `queue_families`;
   决定快照归属(建议 create_device 返回快照,§4)。
4. **`select_physical_device.cpp` / `create_device.cpp`**(新建):实现,`_maythrow`/`noexcept` 边界
   照 `create_instance.cpp`。
5. **Context**:per-role 编排(§5)。DeviceRole/QueueRole 模型、去重、collapse ladder、快照存储
   全在这里。details 三个头都不 include 任何 context/ 或 enums.h(role 枚举)。

不改 CMake(GLOB)。

---

## 7. 一句话总结

拆分可行且正确。判定边界的准绳:**details 原语不需要知道 DeviceRole/QueueRole 就能跑完**。
三个必须守住的契约:select 把 FeatureChain 查询结果(supported_dependencies)传给 create 避免重查;
select 内部 filter(客观)严格先于 sort(policy);create_device 只收 `{family,count}` 机械 queue
描述,collapse ladder 留在 Context。


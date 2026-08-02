# vkc 布局转换设计

> attachment 的入口/出口 layout 转换收进 render 层，静态与动态两条路径共用同一份声明。
> 相关文档：`vkc-dynamic-render-impl.md`、`vkc-attachment-set-authority-design.md`。

## 1. 目标

RenderPass 路径的 layout 转换由 `initialLayout` / `finalLayout` 加驱动补的 external subpass
dependency 隐式完成。动态路径没有这个机制，005 的渲染循环里手写了 28 行
`pipelineBarrier2`（`005_hello_static_pipeline_main.cpp:289-318`）。同一份 attachment set
声明，换条路径要多写 28 行。

本设计要达到：

- 转换的声明单位在两条路径上一致，example 代码逐字相同。
- `DynamicRender::begin/end` 各发一次批量 barrier，每帧零分配。
- 声明的内容是**意图**（这条 pass 前后谁在用这个 attachment），layout 与同步 mask 都从意图
  推出。
- `VK_KHR_unified_image_layouts` 是一个开关，开关不触碰同步逻辑。

## 2. 设计

### 2.1 声明单位：`AttachmentUsage`

一个 attachment 在一条 pass 上有三个位置需要声明：进入前是什么状态、pass 内怎么用、离开后
给谁用。三处用同一个枚举。

```cpp
enum class AttachmentUsage : uint8_t
{
    eNone,                  //- no transition, no barrier
    eDiscard,               //- contents are not preserved
    eColorAttachment,
    eDepthStencilAttachment,
    eDepthStencilReadOnly,
    eTransferSource,
    eTransferDestination,
    eShaderRead,
    ePresent,
};
```

`eNone` 与 `eDiscard` 是两个 sentinel，语义不同：

- `eDiscard`：内容可丢弃，layout 落到 `eUndefined`。**会发 barrier**——必须从 `eUndefined`
  转出。
- `eNone`：未声明，**不发 barrier**。出口侧的默认值，表示转换由调用方自己管。

入口侧不用 `eNone`：入口默认从 `loadOp` 推（`eClear` / `eDontCare` → `eDiscard`，
`eLoad` → 与 in-pass 意图相同）。需要「什么都不做」时把入口意图设成与 in-pass 相同即可，
此时 layout 相等，只留 memory dependency。

### 2.2 一个意图携带五项属性

意图不是只为了算 layout。每个值挂五项：

| 属性 | 用途 |
|---|---|
| `specific_layout` | 常规路径的 `vk::ImageLayout` |
| `unified_layout` | 开了 unified layouts 时的 `vk::ImageLayout` |
| `required_image_usage` | 校验 image 建的时候有没有对应的 `vk::ImageUsageFlagBits` |
| `stage_mask` | barrier 的 `vk::PipelineStageFlags2` |
| `access_mask` | barrier 的 `vk::AccessFlags2` |

关键点：**同步 mask 不经过 layout**。unified layouts 开关只换 layout 那两列里选哪一列，
`stage_mask` / `access_mask` 一个字节不变。

```
                        /-- specific_layout --\
                       /                       >-- to_image_layout() --> vk::ImageLayout
AttachmentUsage --表--<---- unified_layout ---/
                       \-- stage_mask -----------\
                        \-- access_mask ----------> AccessScope
                         \-- required_image_usage -> begin() 时校验
```

`stage_mask` 与 `access_mask` 在表里是两个平铺字段而不是一个 `AccessScope`：属性表所在的
`pipeline/graphics/enums.h` 是叶子头，只依赖 vulkan.hpp 与枚举设施；`AccessScope` 是声明侧的
便利类型，住在 `info_structs.h`（§3.3）。表不该为了一个二元组反向依赖声明层。两者在
`info_structs.h` 里由 `to_access_scope()` 合到一起。

五项属性放**一张表**，一个枚举值一行。加枚举值时表长的 `static_assert` 会失败，而不是某一项
静默返回默认值。

「枚举值 → 一行属性」这个形状用 `utilities/enums/include/enums/enum_attributes_traits.h` 的
`enum_specialized_attributes_traits` 承载：primary template 只声明不定义，每个枚举在自己的头
文件里特化，表与访问器都归特化点（§3.2）。

### 2.3 三层归属

五个转换字段（entry / in-pass / exit 三个意图 + 两个 scope override）全部归 render info。

| 层 | 决定 | 转换相关的新增 |
|---|---|---|
| `AttachmentSetInfo` | 有哪些 attachment、format / sample / load-store op | **无**（只加一个 `makeDefaultTransitions()`） |
| `StaticRenderInfo` / `DynamicRenderInfo` | 这条 pass 怎么用它们 | `m_transitions`（slot indexed）+ `m_unified_layout_enabled` |
| `RenderTarget` | 具体是哪张 image | `viewAttachments()` |

依据是份数：

- 一份 `AttachmentSetInfo` 会被多条 pass 用。同一张 depth 图在 geometry pass 里是
  `eDepthStencilAttachment`、在后续 pass 里是 `eDepthStencilReadOnly`——entry / exit 更是
  逐 pass 不同。放进 set，第二条 pass 无处可写。
- `RenderTarget` 是 per-frame 复数（005 里两个实例共享一份声明）。放进去要复制 N 份并维护
  一致性，白拿一个同步不变量。
- `DynamicRender::begin()` 需要的 `vk::Image` 与 `subresourceRange` 从 `viewAttachments()`
  读即可。读资源不等于持有声明。

结论：`AttachmentSetInfoBuilder` 与 `RenderTargetInfo` **完全不动**，005 的 attachment set 段
一行不改。`AttachmentSetInfo` 只多一个私有工厂 `makeDefaultTransitions()`——它不持有转换状态，
只是因为「哪段是 color、哪段是 resolve、ds 在不在」这套槽位知识本来就归它，两个 render info
的构造函数都从这里拿初值（§3.5）。

### 2.4 unified layouts 开关的位置

`m_unified_layout_enabled` 是 render info 上的一个 bool，由调用方 `enableUnifiedLayouts()`
打开。不引入 policy 类，也不进 `DeviceContext`：

- 消费点只有两个——`DynamicRender::create()` 烘 barrier、`StaticRender::create()` 烘 render
  pass，都在 render info 手上，没有第三方要读。
- 进 `DeviceContext` 意味着声明侧要持有一个指向 device 的 policy 指针，把 attachment / render
  声明的生命周期绑到 device 上。它们现在是纯值类型。
- 一个 bool 包成类，只为那一次三元选择。free function 就够：
  `to_image_layout(usage, unified_enabled)`。

两份 render info 各持一份，理论上可以不一致；但两条 `enableUnifiedLayouts()` 就在声明段相邻
几行，比藏在 device 里的中心策略更容易看出漏了哪个。调用方知道该不该开，因为它自己往
manifest 加了 `unifiedImageLayouts` 且 `DeviceContext::create()` 成功返回。

### 2.5 barrier 的烘制与提交

`create()` 时按 slot 烘好两个 `vk::ImageMemoryBarrier2` 数组，layout 与四个 mask 全部填死，
只留 `image` 与 `subresourceRange` 空着。`begin()` 回填这两个字段并提交入口批次，
`end()` 提交出口批次。

三条优化都落在 `create()`：

**`loadOp` / `storeOp` 驱动的 layout 降级。** `loadOp` 是 `eClear` / `eDontCare` 时入口
`oldLayout` 用 `eUndefined`，驱动跳过解压与内容搬移；`storeOp` 是 `eDontCare` 时出口
`oldLayout` 同样降级。depth-stencil slot 要 depth 与 stencil 两侧都是 discard 类才降级——
只丢一个 aspect 不能把整张图降级。

**空 barrier 跳过。** 入口侧 `oldLayout == newLayout` 且 src mask 为空时不入队；出口侧意图为
`eNone` 时不入队。全部 slot 都跳过时 `pipelineBarrier2` 整个不调。

注意 `oldLayout == newLayout` 但 src mask 非空时**不能**跳过：上一帧可能在同一 layout 下写过
这张图，本帧需要 WAW 依赖。unified layouts 开启后这是常态。

**批量提交。** color + resolve + depth/stencil 打进同一个 `DependencyInfo`。`DynamicRender`
是唯一同时看到全部 slot 的地方，驱动可以合并 cache 操作。

## 3. 实现

### 3.1 变更清单

| 文件 | 动作 | 状态 |
|---|---|---|
| `utilities/enums/include/enums/enum_attributes_traits.h` | 新增，通用设施 | 已实现 |
| `utilities/core/include/concepts/enum_concept.h` | `enum_flags_c` 改用 `enum_c` | 已实现 |
| `utilities/enums/include/enums/enum_name.h` | `enum_name` 加 `constexpr`；flags 版返回 `std::string` | 已实现 |
| `vk_core/.../graphics/enums.h` | 新增：`AttachmentUsage` + 属性表 | 已实现，缺 `static` / 运行期访问器 / `static_assert`（§3.2） |
| `vk_core/.../graphics/info_structs.h` | `AccessScope` + `AttachmentTransitionInfo` + 两个 render info 的 setter 与成员 | 部分：类已在，setter 只有声明（§3.3–§3.5） |
| `vk_core/src/.../info_structs.cpp` | `makeDefaultTransitions` + `resolve_transition` | 待做 |
| `vk_core/.../graphics/RenderTarget.h` | 加 `viewAttachments()` | 待做 |
| `vk_core/.../graphics/DynamicRender.h` / `.cpp` | 加 barrier 成员与烘制 | 待做 |
| `vk_core/.../graphics/entry.h` / `DynamicRender.cpp` | 加 `register_unified_image_layouts` | 待做 |
| `vk_core/src/.../StaticRender.cpp` | `create()` 消费意图 | 待做 |
| `examples/.../005_hello_static_pipeline_main.cpp` | 删 28 行 barrier，加 2 行声明 | 待做 |

`AttachmentSetInfoBuilder` / `RenderTargetInfo` / `AttachmentSetInfo` 的公开接口不动。

### 3.2 `enum_attributes_traits.h`（已实现）

设施拆成两个 traits，一个通用一个特化：

```cpp
namespace lcf {

template <enum_c Enum>
struct enum_common_attributes_traits
{
    template <Enum enum_value>
    inline static constexpr auto index_of_v = std::to_underlying(enum_value);
    template <Enum enum_value>
    inline static auto name_of_v = enum_name<Enum>(enum_value);
    inline static constexpr auto values_v = enum_values_v<Enum>;
};

template <enum_c Enum>
struct enum_specialized_attributes_traits;

} // namespace lcf
```

`enum_common_attributes_traits` 对任何枚举都成立（下标 / 名字 / 值列表），不需要特化。
`enum_specialized_attributes_traits` 是 primary template 只声明不定义——**表和访问器都归特化
点**。这比「统一叫 `rows`、外面配一个泛型 `enum_attributes_of`」的方案好：属性表的字段名是领域
知识，`AttachmentUsage` 要的访问器叫 `layout_of` / `stage_mask_of`，别的枚举要的完全不同；泛型
访问器只能返回整行，调用点还得知道字段名，没省下什么。没特化时编译失败发生在使用点，错误信息
是「incomplete type」，够指向问题。

行序必须与枚举声明序一致，靠特化点的 `static_assert`（§3.2.1）与每行的 `//- eXxx` 注释固定。

#### 3.2.1 两处待补

当前 `enums.h` 的特化能用，但对烘制路径缺两样东西：

**访问器要 `static`。** 现在是非静态 `constexpr` 成员函数，调用点得先造一个实例：

```cpp
lcf::enum_specialized_attributes_traits<vkc::AttachmentUsage>{}.layout_of<usage>()
```

traits 是无状态的，加 `static` 即可，一个词的改动。

**要一组运行期访问器。** 烘制时 `usage` 从 `AttachmentTransitionInfo` 读出来，是运行期值，
喂不进模板参数。表的下标本来就是运行期索引（`get_attributes` 已经是 `std::to_underlying`），
所以只是把同一个私有函数再暴露一组重载：

```cpp
public:
    //- compile-time form: usage known at the call site
    template <lcf::vkc::AttachmentUsage usage>
    static constexpr vk::ImageLayout layout_of(bool unified_enabled = false) noexcept
    {
        return layout_of(usage, unified_enabled);
    }
    template <lcf::vkc::AttachmentUsage usage>
    static constexpr vk::ImageUsageFlags required_image_usage_of() noexcept { return required_image_usage_of(usage); }
    template <lcf::vkc::AttachmentUsage usage>
    static constexpr vk::PipelineStageFlags2 stage_mask_of() noexcept { return stage_mask_of(usage); }
    template <lcf::vkc::AttachmentUsage usage>
    static constexpr vk::AccessFlags2 access_mask_of() noexcept { return access_mask_of(usage); }
    //- run-time form: usage comes from AttachmentTransitionInfo at bake time
    static constexpr vk::ImageLayout layout_of(lcf::vkc::AttachmentUsage usage, bool unified_enabled = false) noexcept
    {
        const auto & attributes = get_attributes(usage);
        return unified_enabled ? attributes.unified_layout : attributes.specific_layout;
    }
    static constexpr vk::ImageUsageFlags required_image_usage_of(lcf::vkc::AttachmentUsage usage) noexcept { return get_attributes(usage).required_image_usage; }
    static constexpr vk::PipelineStageFlags2 stage_mask_of(lcf::vkc::AttachmentUsage usage) noexcept { return get_attributes(usage).stage_mask; }
    static constexpr vk::AccessFlags2 access_mask_of(lcf::vkc::AttachmentUsage usage) noexcept { return get_attributes(usage).access_mask; }
```

两组重载不会歧义：`AttachmentUsage` 是 scoped enum，不隐式转 `bool`，所以
`layout_of(usage, unified)` 只匹配非模板版；`layout_of<usage>()` 要显式模板实参，只匹配模板版。
模板版转发给运行期版，表的访问只有一处。

`static_assert` 也漏了，补在特化的类外：

```cpp
static_assert(std::size(lcf::enum_specialized_attributes_traits<lcf::vkc::AttachmentUsage>::attributes_list) ==
    lcf::enum_count_v<lcf::vkc::AttachmentUsage>);
```

`attributes_list` 现在是 private，`static_assert` 访问不到——要么把它挪到 public，要么在类内加
一行 `static_assert(std::size(attributes_list) == ...)`。**选类内**：表保持 private，断言与表
相邻，加枚举值时报错位置就在表上。需要 `#include "enums/enum_count.h"`。

顺带：`enum_common_attributes_traits::name_of_v` 现在是 `inline static auto`（非 constexpr 的
可变变量模板）。`enum_name` 这次改成 `constexpr` 了，可以一起收成
`inline static constexpr auto`。与本设计无关，但改的是同一个文件。

#### 3.2.2 调用点包装

特化的全名太长，在 `lcf::vkc` 里补两个 free function（`enums.h` 末尾，属性表之后）：

```cpp
constexpr vk::ImageLayout to_image_layout(AttachmentUsage usage, bool unified_enabled = false) noexcept
{
    return lcf::enum_specialized_attributes_traits<AttachmentUsage>::layout_of(usage, unified_enabled);
}

constexpr vk::ImageUsageFlags to_required_image_usage(AttachmentUsage usage) noexcept
{
    return lcf::enum_specialized_attributes_traits<AttachmentUsage>::required_image_usage_of(usage);
}
```

第三个包装 `to_access_scope` 落在 `info_structs.h` 而不是这里（§3.3）——它的返回类型
`AccessScope` 定义在那个文件，`enums.h` 不该为了它反向依赖。

名字不叫 `resolve_layout`：这个文件里 "resolve" 已经被 MSAA resolve attachment 占了
（`ResolveAttachmentKey`、`resolve_mode`、`m_resolve_indices`），`resolve_layout` 会读成
「resolve attachment 的 layout」。`to_xxx` 与 `enum_cast` 那套转换命名一致。

### 3.3 `AccessScope`（已实现，在 `info_structs.h`）

```cpp
struct AccessScope
{
    AccessScope(
        vk::PipelineStageFlags2 stage_mask = {},
        vk::AccessFlags2 access_mask = {}) noexcept :
        m_stage_mask(stage_mask),
        m_access_mask(access_mask) {}
    vk::PipelineStageFlags2 m_stage_mask;
    vk::AccessFlags2 m_access_mask;
};
```

不单开 `utils/AccessScope.h`：唯一的消费者是同文件的 `AttachmentTransitionInfo` 与
`DynamicRender` 的烘制，属性表那边不用它（§2.2）。

有用户提供的构造函数所以不是 aggregate，但两个参数都有默认值，`{eBlit, eTransferRead}` 与 `{}`
两种写法照常可用——005 的声明段就靠这个（§4）。

配一个从意图取 scope 的 free function，接在 `AttachmentTransitionInfo` 之前：

```cpp
inline AccessScope to_access_scope(AttachmentUsage usage) noexcept
{
    using Traits = lcf::enum_specialized_attributes_traits<AttachmentUsage>;
    return { Traits::stage_mask_of(usage), Traits::access_mask_of(usage) };
}
```

这就是 §2.2 里「两个平铺字段在声明层合成 `AccessScope`」的那一步。

### 3.4 `pipeline/graphics/enums.h`（已实现）

枚举与属性表见文件本体。结构是：

```cpp
template <>
struct lcf::enum_specialized_attributes_traits<lcf::vkc::AttachmentUsage>
{
private:
    struct Attributes
    {
        vk::ImageLayout specific_layout;
        vk::ImageLayout unified_layout;
        vk::ImageUsageFlags required_image_usage;
        vk::PipelineStageFlags2 stage_mask;
        vk::AccessFlags2 access_mask;
    };
    static constexpr Attributes attributes_list[] = { /* 一行一个枚举值，序号对齐声明序 */ };
    static_assert(std::size(attributes_list) == lcf::enum_count_v<lcf::vkc::AttachmentUsage>);
    static constexpr const Attributes & get_attributes(lcf::vkc::AttachmentUsage usage) noexcept
    {
        return attributes_list[std::to_underlying(usage)];
    }
public:
    //- §3.2.1: static + run-time overloads
};
```

表与 `get_attributes` 都 private，外面只能走访问器——加字段不会漏改访问器，删字段会在访问器上
报错。

几处填法的理由：

`eUndefined` 与 `ePresentSrcKHR` 的两列相同，不是可选的：`eUndefined` 承载「内容可丢弃 +
初始化 metadata」语义，规范要求保留；`ePresentSrcKHR` 不在扩展覆盖范围（§6）。

同步 mask 三处保守，都可以 override（§3.5）：`eTransferSource` / `eTransferDestination` 用
`eAllTransfer` 而非 `eBlit` / `eCopy` / `eResolve`（意图不区分 transfer 子类）；`eShaderRead`
的 stage 是 `eAllGraphics | eComputeShader` 并集；`ePresent` 全空。`eDiscard` 的 `eAllCommands`
是入口方向的保守默认——丢弃意味着不关心之前谁写的，等所有前序命令是唯一安全解。

`eNone` 那行只写了四个初值，第五项 `access_mask` 靠聚合初始化补零。语义上没错（`eNone` 不发
barrier，整行都读不到），但与其他行不齐；补一个显式 `{}` 更一致。

`required_image_usage` 在 `ePresent` 那行为空：swapchain image 的 usage 位由 swapchain 决定，
不由 render 声明侧校验。`eNone` / `eDiscard` 也为空——两者都不表达具体用法。

`eColorAttachment` 的 access mask 带 `eColorAttachmentRead`（不只 Write），因为 blend 会读。
`eDepthStencilAttachment` 同理带 Read（depth test 会读）。这两处宁可多一位——少了会漏 RAW。

### 3.5 `info_structs.h`

#### 3.5.1 `AttachmentTransitionInfo`（已实现，缺 setter）

已实现的是 class 而非 struct，六个字段走 `m_` + getter。getter 里三个 optional 的形状是
`getXxxOr(fallback)`——烘制点本来就要「有 override 用 override，没有用意图推的值」，
`value_or` 收在类内比在调用点散开好：

```cpp
    AttachmentUsage getEntryUsage() const noexcept;
    AttachmentUsage getInPassUsage() const noexcept;
    AttachmentUsage getExitUsage() const noexcept;
    AccessScope getEntrySrcScopeOr(AccessScope scope = {}) const noexcept;
    AccessScope getExitDstScopeOr(AccessScope scope = {}) const noexcept;
    vk::ImageLayout getInPassLayoutOr(vk::ImageLayout layout = {}) const noexcept;
```

要补的是 setter——render info 的 setter 需要逐字段改已有的元素，只有构造函数改不了：

```cpp
    using Self = AttachmentTransitionInfo; //- 现在这个类没有，同文件其他类都有
public:
    Self & setEntryUsage(AttachmentUsage usage) noexcept { m_entry_usage = usage; return *this; }
    Self & setInPassUsage(AttachmentUsage usage) noexcept { m_in_pass_usage = usage; return *this; }
    Self & setExitUsage(AttachmentUsage usage) noexcept { m_exit_usage = usage; return *this; }
    Self & setEntrySrcScope(const AccessScope & scope) noexcept { m_entry_src_scope_opt = scope; return *this; }
    Self & setExitDstScope(const AccessScope & scope) noexcept { m_exit_dst_scope_opt = scope; return *this; }
    Self & setInPassLayout(vk::ImageLayout layout) noexcept { m_in_pass_layout_opt = layout; return *this; }
```

三个哨兵语义：

- `m_entry_usage == eNone`：**未声明**，烘制时读 `loadOp` 决定（§3.5.4）。所以 `setLoadStoreOp`
  与 `setEntryUsage` 之间没有隐式耦合，调用顺序无关。
- `m_exit_usage == eNone`：**不发 barrier**。
- `m_in_pass_usage == eNone`：非法。构造函数默认值是 `eNone`，但每个 slot 都会被
  `makeDefaultTransitions()`（§3.5.2）填成实际值，所以烘制时看到 `eNone` 意味着调用方显式写了
  `setInPassUsage(key, eNone)`，断言拦掉。

`eNone` 在三个字段上三个意思，但每个字段上唯一，不会歧义。`m_in_pass_layout_opt` 是 escape
hatch，绕过 `m_in_pass_usage` 直接给 layout；此时同步 mask 仍从 `m_in_pass_usage` 推，所以两者
不一致时要配 `setEntrySrcScope` / `setExitDstScope` 手动补。

#### 3.5.2 槽位默认值：`AttachmentSetInfo::makeDefaultTransitions()`

`m_transitions` 按 slot 索引，长度等于 attachment 数。初值按段不同，而「哪段是什么」只有
`AttachmentSetInfo` 知道（它持有 `[colors][resolves][ds?]` 的布局与 `m_color_resolve_list`），
所以工厂放它这儿，两个 render info 的构造函数都调它：

```cpp
class AttachmentSetInfo
{
    using TransitionList = std::vector<AttachmentTransitionInfo>;
    ...
private:
    TransitionList makeDefaultTransitions() const noexcept;
};
```

```cpp
auto AttachmentSetInfo::makeDefaultTransitions() const noexcept -> TransitionList
{
    uint32_t attachment_count = static_cast<uint32_t>(m_descriptions.size());
    uint32_t color_count = this->getColorAttachmentCount();
    uint32_t resolve_end = attachment_count - m_has_depth_stencil;
    TransitionList transitions(attachment_count);
    for (uint32_t slot = 0; slot < color_count; ++slot) {
        transitions[slot].setInPassUsage(AttachmentUsage::eColorAttachment);
    }
    for (uint32_t slot = color_count; slot < resolve_end; ++slot) {
        //- a resolve target is fully overwritten by the resolve op, so its entry intent is
        //- eDiscard outright rather than derived from loadOp: RenderingAttachmentInfo carries
        //- no load op for the resolve side, and the set builder leaves it at the eLoad default
        transitions[slot].setInPassUsage(AttachmentUsage::eColorAttachment)
            .setEntryUsage(AttachmentUsage::eDiscard);
    }
    if (m_has_depth_stencil) {
        transitions.back().setInPassUsage(AttachmentUsage::eDepthStencilAttachment);
    }
    return transitions;
}
```

resolve 段那条注释是必要的：走 `loadOp` 推的话，builder 建 resolve description 时没设 loadOp，
拿到的是 `vk::AttachmentLoadOp::eLoad` 默认值，会推出 `eColorAttachment`，于是首帧 `oldLayout`
写成 `eColorAttachmentOptimal` 而实际是 `eUndefined`——validation 报 layout 不匹配。

#### 3.5.3 两个 render info 的接口

各加同一组七个 setter（`StaticRenderInfo` 现在完全没有，`DynamicRenderInfo` 有声明没定义）：

```cpp
    Self & enableUnifiedLayouts() noexcept { m_unified_layout_enabled = true; return *this; }
    //- entry defaults from loadOp; exit defaults to eNone (no barrier emitted)
    Self & setEntryUsage(details::attachment_key_c auto key, AttachmentUsage usage) noexcept
    {
        m_transitions[m_attachments.getIndex(key)].setEntryUsage(usage);
        return *this;
    }
    Self & setInPassUsage(details::attachment_key_c auto key, AttachmentUsage usage) noexcept
    {
        m_transitions[m_attachments.getIndex(key)].setInPassUsage(usage);
        return *this;
    }
    Self & setExitUsage(details::attachment_key_c auto key, AttachmentUsage usage) noexcept
    {
        m_transitions[m_attachments.getIndex(key)].setExitUsage(usage);
        return *this;
    }
    //- overrides the scope derived from the usage above
    Self & setEntrySrcScope(details::attachment_key_c auto key, const AccessScope & scope) noexcept
    {
        m_transitions[m_attachments.getIndex(key)].setEntrySrcScope(scope);
        return *this;
    }
    Self & setExitDstScope(details::attachment_key_c auto key, const AccessScope & scope) noexcept
    {
        m_transitions[m_attachments.getIndex(key)].setExitDstScope(scope);
        return *this;
    }
    //- escape hatch: bypasses in_pass_usage for the layout only, masks still come from it
    Self & setInPassLayout(details::attachment_key_c auto key, vk::ImageLayout layout) noexcept
    {
        m_transitions[m_attachments.getIndex(key)].setInPassLayout(layout);
        return *this;
    }
private:
    TransitionList m_transitions; //- slot indexed
    bool m_unified_layout_enabled = false;
```

七个一行 setter 在两个类里重复。不抽 CRTP mixin 或公共基类：`m_attachments` 与 `m_transitions`
的归属、`Self` 的类型、友元声明都要穿过基类，为 14 行重复引入一层模板继承不划算。真正会长的
逻辑（默认值、烘制）已经分别收在 §3.5.2 与 §3.5.4。

`getIndex` 是 `AttachmentSetInfo` 的私有成员，两个 render info 都已经是它的友元，直接调即可；
它内部带 set id 断言，跨 set 的 key 会被拦。

**`DynamicRenderInfo` 要删的：** 现有的 `m_layouts` 数组、`LayoutList` 别名、`getLayouts()`，
以及那个已定义的 `setLayout`。in-pass layout 改成烘制时算，`RenderingAttachmentInfo::imageLayout`
与 barrier 的 `newLayout` 取同一个值，不会写歪。

注意现在文件里 `setLayout` 出现了两次——一次带定义（写 `m_layouts`），一次是 §3.5 那组里的裸
声明。C++ 允许成员函数重复声明，所以不是编译错误，但那个裸声明永远没有定义，一旦被调就是链接
错误。两个都删，换成 `setInPassLayout`。

**`DynamicRenderInfo` 要加的：**

```cpp
    Self & enableByRegionDependency() noexcept { m_by_region_enabled = true; return *this; }
private:
    bool m_by_region_enabled = false;
```

`eByRegion` 只加在入口 barrier 上——出口的消费者（blit / transfer）通常在 framebuffer 空间
之外。只在所有前序 producer 写的是同一像素坐标时成立，所以是 opt-in。

两个构造函数都改成从 §3.5.2 取初值：

```cpp
    explicit DynamicRenderInfo(AttachmentSetInfo & attachments) noexcept :
        m_attachments(attachments),
        m_transitions(attachments.makeDefaultTransitions()) {}

    explicit StaticRenderInfo(AttachmentSetInfo & attachments) noexcept :
        m_attachments(attachments),
        m_transitions(attachments.makeDefaultTransitions()) {}
```

`DynamicRenderInfo` 原来在构造函数体里给 ds slot 改 `m_layouts.back()` 的那段一起删掉。

#### 3.5.4 烘制：`resolve_transition()`

两条路径都要把「意图 + loadOp/storeOp」化成具体 layout 与 mask，所以这一步收成一个自由函数，
放 `info_structs.h`（`DynamicRender.cpp` 与 `StaticRender.cpp` 都要用）：

```cpp
struct ResolvedTransition
{
    vk::ImageLayout entry_layout;
    vk::ImageLayout in_pass_layout;
    vk::ImageLayout exit_layout;      //- eUndefined when exit_usage is eNone
    vk::ImageLayout exit_old_layout;  //- in_pass_layout, or eUndefined when storeOp discards
    AccessScope entry_src_scope;
    AccessScope in_pass_scope;
    AccessScope exit_dst_scope;
    vk::ImageUsageFlags required_image_usage; //- union of the three intents
    bool entry_barrier_needed;
    bool exit_barrier_needed;
};

ResolvedTransition resolve_transition(
    const AttachmentDescriptionInfo & description,
    const AttachmentTransitionInfo & transition,
    bool unified_enabled) noexcept;
```

定义在 `info_structs.cpp`，配两个文件级判定：

```cpp
namespace {

bool discards_on_load(const AttachmentDescriptionInfo & description) noexcept
{
    constexpr auto is_discard = [](vk::AttachmentLoadOp op) noexcept {
        return op == vk::AttachmentLoadOp::eClear or op == vk::AttachmentLoadOp::eDontCare;
    };
    vk::Format format = description.getFormat();
    //- a depth-stencil slot only downgrades when every aspect it actually has is discarded;
    //- Vulkan ignores stencilLoadOp on a depth-only format, so don't let its default veto
    bool depth_discarded = not utils::is_depth_format(format) or is_discard(description.getLoadOp());
    bool stencil_discarded = not utils::is_stencil_format(format) or is_discard(description.getStencilLoadOp());
    if (utils::is_depth_format(format) or utils::is_stencil_format(format)) {
        return depth_discarded and stencil_discarded;
    }
    return is_discard(description.getLoadOp());
}

bool discards_on_store(const AttachmentDescriptionInfo & description) noexcept
{
    //- eNone is not a discard: it leaves contents either preserved or undefined, and treating
    //- "maybe preserved" as discardable would throw away data the caller may still want
    constexpr auto is_discard = [](vk::AttachmentStoreOp op) noexcept {
        return op == vk::AttachmentStoreOp::eDontCare;
    };
    vk::Format format = description.getFormat();
    bool depth_discarded = not utils::is_depth_format(format) or is_discard(description.getStoreOp());
    bool stencil_discarded = not utils::is_stencil_format(format) or is_discard(description.getStencilStoreOp());
    if (utils::is_depth_format(format) or utils::is_stencil_format(format)) {
        return depth_discarded and stencil_discarded;
    }
    return is_discard(description.getStoreOp());
}

} // anonymous namespace
```

按 format 判 aspect 而不是靠调用方传 `is_depth_stencil` 布尔：ds slot 的 format 已经在
description 里，多一个参数就多一处能传错的地方。depth-only format 上 `stencilLoadOp` 是被
Vulkan 忽略的字段，默认值 `eLoad` 不该否掉降级。

```cpp
ResolvedTransition resolve_transition(
    const AttachmentDescriptionInfo & description,
    const AttachmentTransitionInfo & transition,
    bool unified_enabled) noexcept
{
    AttachmentUsage in_pass_usage = transition.getInPassUsage();
    assert(in_pass_usage != AttachmentUsage::eNone and
        "in-pass usage must name a real usage; eNone would resolve the layout to eUndefined");
    AttachmentUsage entry_usage = transition.getEntryUsage();
    if (entry_usage == AttachmentUsage::eNone) {
        //- undeclared: derive from loadOp
        entry_usage = discards_on_load(description) ? AttachmentUsage::eDiscard : in_pass_usage;
    }
    AttachmentUsage exit_usage = transition.getExitUsage();

    ResolvedTransition resolved;
    resolved.in_pass_layout = transition.getInPassLayoutOr(to_image_layout(in_pass_usage, unified_enabled));
    resolved.in_pass_scope = to_access_scope(in_pass_usage);
    resolved.entry_layout = to_image_layout(entry_usage, unified_enabled);
    resolved.entry_src_scope = transition.getEntrySrcScopeOr(to_access_scope(entry_usage));
    resolved.exit_layout = to_image_layout(exit_usage, unified_enabled);
    resolved.exit_dst_scope = transition.getExitDstScopeOr(to_access_scope(exit_usage));
    resolved.exit_old_layout = discards_on_store(description)
        ? vk::ImageLayout::eUndefined : resolved.in_pass_layout;
    resolved.required_image_usage = to_required_image_usage(entry_usage) |
        to_required_image_usage(in_pass_usage) |
        to_required_image_usage(exit_usage);
    //- an entry barrier with matching layouts is still needed when the src scope is non-empty:
    //- the previous frame may have written this image in the same layout (WAW). With unified
    //- layouts on, that is the common case rather than the exception.
    resolved.entry_barrier_needed = resolved.entry_layout != resolved.in_pass_layout or
        static_cast<bool>(resolved.entry_src_scope.m_stage_mask);
    resolved.exit_barrier_needed = exit_usage != AttachmentUsage::eNone;
    return resolved;
}
```

`getStencilLoadOp` / `getStencilStoreOp` 已在 `info_structs.h:747-748`，不用补。

`exit_layout` 在 `exit_usage == eNone` 时是 `eUndefined`（属性表 `eNone` 行的
`specific_layout`），但那种情况下 `exit_barrier_needed` 为 false，这个值不会被读。留着而不是
包 optional：`ResolvedTransition` 是纯计算中间体，两个 `_needed` 标志已经说明哪些字段有效。

#### 3.5.5 `StaticRenderInfo` 的消费方式

`StaticRender::create()` 把 entry / exit layout 写进 `AttachmentDescriptionInfo` 的
`initialLayout` / `finalLayout`，`in_pass_layout` 成为 `AttachmentReferenceInfo` 的 layout。
mask 保持隐式（驱动补 external dependency），所以 §3.5.4 算出的 scope 在静态路径上用不到——
这是有意的，静态路径先接是动态路径的安全网（§8）。

与现有 `setInitialFinalLayout` 的优先级：**该 slot 声明了 entry / exit 意图时以意图为准**，
未声明的保留手写 layout。按声明存在性判定，不按调用顺序。

一处与动态路径的不对称：**静态路径不做 `loadOp` 驱动的 entry 降级**。`m_entry_usage == eNone`
时静态路径就是「不覆盖 `initialLayout`」，而不是推成 `eDiscard`。理由是这个降级本质是 barrier
优化，而静态路径的 `initialLayout` 若没人写就已经是 `eUndefined`（`vk::AttachmentDescription2`
的默认值），推它反而会覆盖调用方手写的值。resolve 段的 `eDiscard`（§3.5.2 显式设的）算「已
声明」，会覆盖成 `eUndefined`——那正是 resolve target 该有的初始 layout。

`in_pass_layout` 落进 `AttachmentReferenceInfo` 需要在 subpass 构造时就知道，而 subpass 是调用方
自己建好 move 进来的。所以给 `StaticRenderInfo` 加一个转发工厂，代替直接调
`attachment_set.makeAttachmentReference(key)`：

```cpp
    AttachmentReferenceInfo makeAttachmentReference(details::attachment_key_c auto key) const noexcept
    {
        const auto & transition = m_transitions[m_attachments.getIndex(key)];
        vk::ImageLayout layout = transition.getInPassLayoutOr(
            to_image_layout(transition.getInPassUsage(), m_unified_layout_enabled));
        return m_attachments.makeAttachmentReference(key, layout);
    }
```

`AttachmentSetInfo::makeAttachmentReference` 那三个重载保留不动（layout 有默认值，仍可单独用）。
这个转发版有顺序要求：**必须在该 slot 的 `setInPassUsage` / `enableUnifiedLayouts` 之后调**，
因为它当场读值。这是唯一一处顺序敏感的接口，写在注释里。

### 3.6 `RenderTarget.h`

```cpp
    std::span<const Attachment> viewAttachments() const noexcept { return m_attachments; }
```

`Attachment` 是 move-only，但 `span<const Attachment>` 只读不拷。`getImage()` 返回
`const Image &`，`Image` 有 `operator vk::Image()`；`getDescription().getSubresourceRange()`
给 aspect / mip / layer。

现有的 `viewAttachmentImageViews()`（`views::transform` 出 image view）保留——`StaticRender`
建 framebuffer 用它，`DynamicRender::begin()` 填 `RenderingAttachmentInfo` 也用它。新接口只服务
barrier 回填，因为那里要的是 image 与 range 而不是 view。

### 3.7 `DynamicRender` 的 barrier

新增成员：

```cpp
    struct BarrierSlotMap
    {
        uint32_t slot = vk::AttachmentUnused;
        uint32_t entry_index = vk::AttachmentUnused;  //- index into m_entry_barriers
        uint32_t exit_index = vk::AttachmentUnused;   //- index into m_exit_barriers
        vk::ImageUsageFlags required_image_usage;     //- checked against the image on begin()
    };
    using BarrierList = std::vector<vk::ImageMemoryBarrier2>;
    using BarrierSlotMapList = std::vector<BarrierSlotMap>;
private:
    BarrierList m_entry_barriers;
    BarrierList m_exit_barriers;
    BarrierSlotMapList m_barrier_slot_maps; //- one per slot needing any barrier
    vk::DependencyFlags m_dependency_flags;
```

入口与出口必须是两个数组：`pipelineBarrier2` 要求 `VkImageMemoryBarrier2` 连续，而两者是两次
调用。映射表只需一张——同一 slot 的两条 barrier 共用 image 与 range，一次循环回填两边。

`required_image_usage` 挂在映射表上而不是单开一个 per-slot 数组：只有会发 barrier 的 slot 需要
校验，而完全不发 barrier 的 slot（意图全 `eNone`）本来就不表达用法，没什么可校验。

`create()` 的完整形状——原有的 `RenderingAttachmentInfo` 填充与 barrier 烘制合成一遍循环，
因为两者都要 `in_pass_layout`：

```cpp
std::error_code DynamicRender::create(const DynamicRenderInfo & render_info) noexcept
{
    m_rendering = render_info.m_rendering;
    const auto & descriptions = render_info.getAttachmentDescriptions();
    const auto & color_resolve_list = render_info.getColorResolveList();
    const auto & transitions = render_info.getTransitions();
    bool unified_enabled = render_info.isUnifiedLayoutEnabled();
    uint32_t color_count = render_info.getColorAttachmentCount();
    uint32_t attachment_count = static_cast<uint32_t>(descriptions.size());
    m_color_attachments.clear();
    m_color_formats.clear();
    m_resolve_indices.clear();
    m_entry_barriers.clear();
    m_exit_barriers.clear();
    m_barrier_slot_maps.clear();
    m_color_attachments.reserve(color_count);
    m_color_formats.reserve(color_count);
    m_resolve_indices.reserve(color_count);
    m_barrier_slot_maps.reserve(attachment_count);
    m_dependency_flags = render_info.isByRegionEnabled() ? vk::DependencyFlagBits::eByRegion
                                                        : vk::DependencyFlags {};

    //- one resolve per slot, reused by both the rendering info and the barriers
    std::vector<ResolvedTransition> resolved;
    resolved.reserve(attachment_count);
    for (uint32_t slot = 0; slot < attachment_count; ++slot) {
        resolved.emplace_back(resolve_transition(descriptions[slot], transitions[slot], unified_enabled));
    }

    for (uint32_t color_index = 0; color_index < color_count; ++color_index) {
        const AttachmentDescriptionInfo & description = descriptions[color_index];
        auto [resolve_mode, resolve_index] = color_resolve_list[color_index];
        m_color_attachments.emplace_back()
            .setImageLayout(resolved[color_index].in_pass_layout)
            .setResolveMode(resolve_mode)
            .setLoadOp(description.getLoadOp())
            .setStoreOp(description.getStoreOp());
        if (resolve_index != vk::AttachmentUnused) {
            m_color_attachments.back().setResolveImageLayout(resolved[resolve_index].in_pass_layout);
        }
        m_resolve_indices.emplace_back(resolve_index);
        m_color_formats.emplace_back(description.getFormat());
    }
    if (render_info.hasDepthStencilAttachment()) {
        const AttachmentDescriptionInfo & description = descriptions.back();
        vk::RenderingAttachmentInfo depth_stencil_attachment;
        depth_stencil_attachment.setImageLayout(resolved.back().in_pass_layout)
            .setLoadOp(description.getLoadOp())
            .setStoreOp(description.getStoreOp());
        if (utils::is_depth_format(description.getFormat())) {
            m_depth_attachment = depth_stencil_attachment;
            m_depth_format = description.getFormat();
        }
        if (utils::is_stencil_format(description.getFormat())) {
            m_stencil_attachment = depth_stencil_attachment;
            m_stencil_format = description.getFormat();
        }
    }
    this->bakeBarriers(resolved);
    return {};
}
```

原来 ds 段前那句 `if (not render_info.hasDepthStencilAttachment()) { return {}; }` 的提前返回
要换成 `if` 块——后面还有 `bakeBarriers`。这是接线时最容易漏的一处。

stencil 那条 `RenderingAttachmentInfo` 沿用 depth 的 `loadOp` / `storeOp`（现有行为，不改）：
ds 合一 format 时两侧共用一份 description，`setStencilLoadStoreOp` 写进去的值现在还没被
`DynamicRender` 读。这是既有遗留，记在 §9。

barrier 烘制单独一个私有方法，每个 slot 一段：

```cpp
void DynamicRender::bakeBarriers(std::span<const ResolvedTransition> resolved) noexcept
{
    for (auto && [slot, transition] : resolved | stdv::enumerate) {
        BarrierSlotMap slot_map;
        slot_map.slot = static_cast<uint32_t>(slot);
        slot_map.required_image_usage = transition.required_image_usage;
        if (transition.entry_barrier_needed) {
            slot_map.entry_index = static_cast<uint32_t>(m_entry_barriers.size());
            m_entry_barriers.emplace_back()
                .setOldLayout(transition.entry_layout)
                .setNewLayout(transition.in_pass_layout)
                .setSrcStageMask(transition.entry_src_scope.m_stage_mask)
                .setSrcAccessMask(transition.entry_src_scope.m_access_mask)
                .setDstStageMask(transition.in_pass_scope.m_stage_mask)
                .setDstAccessMask(transition.in_pass_scope.m_access_mask);
        }
        if (transition.exit_barrier_needed) {
            slot_map.exit_index = static_cast<uint32_t>(m_exit_barriers.size());
            m_exit_barriers.emplace_back()
                .setOldLayout(transition.exit_old_layout)
                .setNewLayout(transition.exit_layout)
                .setSrcStageMask(transition.in_pass_scope.m_stage_mask)
                .setSrcAccessMask(transition.in_pass_scope.m_access_mask)
                .setDstStageMask(transition.exit_dst_scope.m_stage_mask)
                .setDstAccessMask(transition.exit_dst_scope.m_access_mask);
        }
        if (slot_map.entry_index != vk::AttachmentUnused or slot_map.exit_index != vk::AttachmentUnused) {
            m_barrier_slot_maps.emplace_back(slot_map);
        }
    }
}
```

`DynamicRenderInfo` 要给 `DynamicRender` 新开三个读取口（它已经是友元）：`getTransitions()`、
`isUnifiedLayoutEnabled()`、`isByRegionEnabled()`。`getAttachmentDescriptions()` /
`getColorResolveList()` / `hasDepthStencilAttachment()` / `getColorAttachmentCount()` 已有，
`getLayouts()` 删掉。

`vk::AttachmentUnused` 当哨兵：它是 `~0u`，与 barrier 数组的合法下标不冲突。

被替代的旧设计片段（原 §3.7 的行内烘制、`discards_on_load(desc, is_depth_stencil)` 双参签名、
散在 `create()` 里的 `enum_attributes_of`）已经收进 §3.5.4 与这里，不再单列。

队列族索引不设，默认 `vk::QueueFamilyIgnored`——同队列无所有权转移，与 `Swapchain.cpp` 一致。

`begin()` 在现有实现前面插一段回填与提交：

```cpp
void DynamicRender::begin(CommandBufferProxy & cmd, const RenderTarget & render_target) noexcept
{
    auto attachments = render_target.viewAttachments();
    //- the exit barriers reference the same attachments, so fill both sides here; end() then
    //- needs no render target parameter
    for (const auto & slot_map : m_barrier_slot_maps) {
        const Attachment & attachment = attachments[slot_map.slot];
        vk::Image image = attachment.getImage();
        vk::ImageSubresourceRange range = attachment.getDescription().getSubresourceRange();
        assert((attachment.getImage().getDescription().getUsageFlags() & slot_map.required_image_usage) ==
            slot_map.required_image_usage and
            "attachment image lacks a usage bit required by its declared transition intents");
        if (slot_map.entry_index != vk::AttachmentUnused) {
            m_entry_barriers[slot_map.entry_index].setImage(image).setSubresourceRange(range);
        }
        if (slot_map.exit_index != vk::AttachmentUnused) {
            m_exit_barriers[slot_map.exit_index].setImage(image).setSubresourceRange(range);
        }
    }
    if (not m_entry_barriers.empty()) {
        vk::DependencyInfo dependency_info;
        dependency_info.setDependencyFlags(m_dependency_flags)
            .setImageMemoryBarriers(m_entry_barriers);
        cmd.pipelineBarrier2(dependency_info);
    }
    //- ... 现有的 per-frame attachment info 回填 + beginRendering，不动
}

void DynamicRender::end(CommandBufferProxy & cmd) noexcept
{
    cmd.endRendering();
    if (m_exit_barriers.empty()) { return; }
    vk::DependencyInfo dependency_info;
    dependency_info.setImageMemoryBarriers(m_exit_barriers);
    cmd.pipelineBarrier2(dependency_info);
}
```

入口 barrier 必须在 `beginRendering` **之前**提交——rendering scope 内不能发 image barrier。
所以回填与提交都在现有代码之前，而不是穿插进去。

出口 barrier 不带 `m_dependency_flags`：`eByRegion` 只对入口有意义（§3.5.3）。

`vk::DependencyInfo` 在栈上构造，不做成员——做成员的话 `pImageMemoryBarriers` 指向
`m_entry_barriers` 的缓冲区，`DynamicRender` 一旦被 move 就悬空。每帧一次
`setImageMemoryBarriers` 的成本可忽略，与 `m_rendering` 现有的 `setColorAttachments` 同一
处理方式。

usage 位校验就在这个循环里：三个意图的 `required_image_usage` 并集必须被
`Image::getDescription().getUsageFlags()` 覆盖。`create()` 拿不到 `RenderTarget`，所以只能落在
`begin()`。不额外记「是否已校验」——`assert` 在 release 下整句消失，debug 下每帧多几个位运算，
不值得为省这点开销引入一个 mutable 标志。

**线程模型**：`begin()` 写 `m_entry_barriers` / `m_exit_barriers`，所以一个 `DynamicRender`
实例不能被多个并发录制线程共用。这不是新增约束——现有的 `begin()` 已经在写
`m_color_attachments` 与 `m_depth_attachment`。多线程录制要每线程一份实例。

### 3.8 `register_unified_image_layouts`

`entry.h` 加声明，`DynamicRender.cpp` 的 `entry` namespace 里定义：

```cpp
void register_unified_image_layouts(DeviceExtensionManifest & manifest) noexcept
{
    static constexpr std::array k_features
    {
        LCF_VKC_UTILS_FEATURE_BIT(&vk::PhysicalDeviceUnifiedImageLayoutsFeaturesKHR::unifiedImageLayouts),
    };
    manifest.addRequiredExtension(vk::KHRUnifiedImageLayoutsExtensionName)
        .addRequiredFeatures(k_features);
}
```

`unifiedImageLayoutsVideo` 不注册——video layout 由独立 feature 位控制，本设计不涉及。

## 4. 005 示例

attachment set / render target / image 段（`:167-207`）不动。声明段两条路径的意图部分逐字相同：

```cpp
    static_render_info.setLoadStoreOp(color_key, vk::AttachmentLoadOp::eClear, vk::AttachmentStoreOp::eStore)
        .setExitUsage(color_key, vkc::AttachmentUsage::eTransferSource)
        .setExitDstScope(color_key, {vk::PipelineStageFlagBits2::eBlit, vk::AccessFlagBits2::eTransferRead});
    //- 意图声明必须在 makeAttachmentReference 之前——见下
    subpass_info.addColorAttachmentReference(static_render_info.makeAttachmentReference(color_key));
    static_render_info.addSubpass(std::move(subpass_info));
```

```cpp
    dynamic_render_info.setLoadStoreOp(color_key, vk::AttachmentLoadOp::eClear, vk::AttachmentStoreOp::eStore)
        .setExitUsage(color_key, vkc::AttachmentUsage::eTransferSource)
        .setExitDstScope(color_key, {vk::PipelineStageFlagBits2::eBlit, vk::AccessFlagBits2::eTransferRead});
```

入口不写——`loadOp = eClear` 推出 `eDiscard`。`setExitDstScope` 那行可省，省了就是
`eAllTransfer`，比 `eBlit` 保守一档。

静态路径原先的 `attachment_set.makeAttachmentReference(color_key)`（`:214`）换成
`static_render_info.makeAttachmentReference(color_key)`（§3.6）——引用里的 layout 要取
in-pass 意图，只有 `StaticRenderInfo` 知道。**这是全套 API 里唯一一处顺序敏感的调用**：
`makeAttachmentReference` 按值读当时的 `m_transitions[slot]`，之后再调
`setInPassUsage` / `setInPassLayout` 不会回改已生成的引用。

`setInitialFinalLayout(color_key, eUndefined, eTransferSrcOptimal)`（`:217`）整行删除——
entry / exit 意图已经表达了同样的事，且 `finalLayout` 由 `eTransferSource` 那行推出。

渲染循环 `:289-318` 的 28 行全删，剩下：

```cpp
    dynamic_render.begin(cmd, render_target);
    dynamic_graphics_pipeline.bind(cmd);
    cmd.draw(3, 1, 0, 0);
    dynamic_render.end(cmd);
```

`present_image`（`:328`）恢复成从 `render_target` 直接取。

**跨帧行为**：帧 0 的 image 从 `eUndefined` 进来；帧 N>0 时上一帧的出口 barrier 把它留在
`eTransferSrcOptimal`，present 的 blit 只读不改 layout。入口意图都是 `eDiscard`
（`eUndefined`），两种情况都合法——`loadOp = eClear` 不需要旧内容。所以循环不需要追踪实际
layout。

一旦某帧改成 `loadOp = eLoad`，入口意图自动变成 `in_pass_usage`，此时 `oldLayout` 是
`eColorAttachmentOptimal` 而实际是 `eTransferSrcOptimal`——**必须显式
`setEntryUsage(color_key, eTransferSource)`**。写错的话 validation layer 会报 layout
不匹配的 VUID。

## 5. 不做自动 layout 追踪

在 `Image` / `Attachment` 上挂 `m_current_layout` 让 `begin()` 自己读上一次状态——不做：

- `RenderTarget` 按 `const &` 传，追踪需要 mutable 状态。
- 多帧 in-flight 时同一张 image 会被不同线程的 record 看到不一致的状态。layout 的正确顺序是
  **command buffer 内的记录顺序**，不是墙上时间，per-resource 单变量表达不了。
- 这套东西最后会被 RenderGraph 的 pass 级追踪替掉。

意图声明是显式的，帧间不携带状态。

## 6. unified image layouts 的边界

SDK 1.4.341 起可用。开启后 `eGeneral` 可用于原本需要特定 layout 的绝大多数位置，且规范承诺
无效率损失。

**继续填具体 layout 不会有任何变化。** 规范说这些 layout「effectively all identical」、
「transitions between them are unnecessary」，描述的是**物理层面**——在支持该 feature 的硬件上
它们映射到同一物理布局，转换在驱动内部是 no-op。但 API 层的状态跟踪没有放松：提案的
issue 4.7 明确否掉了「让 layout 参数被忽略」这个方案，理由是要让不感知 layout 的 helper 库能
与开了扩展的 app 共存。推论：

- image 停在 `eColorAttachmentOptimal` 不加转换直接拿去 blit 仍是 UB。
- 混用具体 layout 与 `eGeneral` 合法但零收益，转换的 API 开销照付。
- 要拿收益必须主动全线改 `eGeneral`。`to_image_layout()` 就是那个「主动改」的落地点。

**不能省掉的：**

- **memory dependency 照旧。** 扩展干掉的是 layout 要求，不是同步要求。
- **image barrier 仍优于 global barrier。** 提案明确：即使 src 与 dst layout 都是 `eGeneral`，
  部分硬件上 image barrier 仍是最佳性能所必需。所以 §3.7 不改用 `vk::MemoryBarrier2`。
- **`eUndefined → eGeneral` 的首次转换还得有。**
- **present 仍需 `ePresentSrcKHR`。** compositor 在 Vulkan 之外。
- **驱动支持面窄。** 桌面 AMD / NV 有，移动端 tiler 因 framebuffer 压缩不一定给。特定 layout
  那条路径不能退化成没人测的死代码——它才是移动端会走的。

**开启后的收益：** 跨 pass 的入口 barrier 变成 `oldLayout == newLayout == eGeneral`，退化成纯
memory dependency，驱动不做解压、不刷压缩元数据。mask 完全不受影响。

**阻塞项：** `Swapchain.cpp` 有四处硬编码 layout（`:92`、`:100`、`:118`、`:120`），其中 blit
的源 layout 写死 `eTransferSrcOptimal`。render target 的出口意图解析成 `eGeneral` 时不匹配，
按上面是 UB。这四处要改走 `to_image_layout` 或由调用方传入，才能开 unified。在此之前
`register_unified_image_layouts` 不接线。

## 7. 范围外

- **跨 pass barrier 合并。** pass A 的出口与 pass B 的入口是同一转换时应合成一条。需要
  RenderGraph 的 pass 依赖图。
- **subresource 收窄。** 现在整个 attachment 的 range 一起转换。
- **`eByRegion` 自动判定。** 需要知道前序 producer 是否 framebuffer-local。
- **静态路径的显式 external dependency。** `m_transitions` 里的 scope 信息已经现成，可以直接
  喂 `SubpassDependencyInfo`，但要决定与多 subpass 的交互，另开设计。
- **queue family ownership transfer。** 现在全部 `eQueueFamilyIgnored`。
- **`to_image_layout` 扩到非 attachment image。** 采样纹理、storage image 的 layout 也该过同一
  个函数，否则 unified 只覆盖一半。等纹理路径落地后再做。
- **usage 位校验时机。** §3.7 那条断言落在 `begin()`，每帧都跑。想在 `create()` 时报，需要多传
  一个 usage flags 列表，或让 `RenderTargetInfo` 记下每个 slot 的 usage 位。
- **`enum_specialized_attributes_traits` 复用到 `DescriptorSetIndex`。** 那套
  `[strategy:4][index:4]` 编码载荷小、是笛卡尔积，位编码在那里可能仍是对的，只共享「decode
  函数散开」这个较弱的问题。等描述符重构那条线动到时再评估。

## 8. 落地顺序

**已落地**：`enum_attributes_traits.h`（§3.2）、`pipeline/graphics/enums.h` 的枚举与属性表
（§3.1、§3.2.1）、`info_structs.h` 里的 `AccessScope` 与 `AttachmentTransitionInfo` 骨架
（§3.3、§3.5.1）。

剩下的：

1. **补 `enums.h`**：accessor 加 `static`、加运行时重载、把 `static_assert` 放进类内
   （§3.2.1）。纯 std + vulkan.hpp，可单独编过。
2. `to_image_layout` + `to_access_scope`（§3.2.2、§3.3）。
3. `AttachmentSetInfo::makeDefaultTransitions()`（§3.4）+ `RenderTarget::viewAttachments()`。
4. `resolve_transition()` 与 `ResolvedTransition`（§3.5.4）——两条路径共用，先它再上层。
5. `StaticRenderInfo` 的 setter 与成员 + `makeAttachmentReference` +
   `StaticRender::create()` 消费意图（§3.6）—— **静态路径先接是动态路径的安全网**：静态路径有
   驱动的隐式 barrier 兜底，属性表填错会体现在 layout 上而不是同步上，更容易定位。005 静态路径
   跑通、画面不变即验证通过。
6. `DynamicRenderInfo` 收尾：`m_transitions` 在构造里定尺寸、7 个 setter 补定义、删两个
   `setLayout` 与 `m_layouts`（§3.5.2）。
7. `DynamicRender` 烘 barrier + `begin` / `end`（§3.7）。
8. 005 删 28 行 barrier，动态路径跑通。开 validation layer，重点看
   `VUID-vkCmdBeginRendering-*`、`VUID-VkImageMemoryBarrier2-oldLayout-*` 与 sync validation
   的 hazard 报告。
9. 可延后：`Swapchain` 四处 layout 改走 `to_image_layout`（§6），然后
   `register_unified_image_layouts` 接线（§3.8）。需要支持的驱动才好验证。

`libs/vk_core/CMakeLists.txt` 用 `GLOB_RECURSE`，新增 .cpp 不改 CMake，但要重跑 configure。

## 9. 已知遗留

- `RenderTarget` 缺 `setResolveAttachment` / `setDepthStencilAttachment`，所以 resolve 段与
  ds slot 的 barrier 路径在 005 里跑不到，只有单 color 被覆盖。批量提交（§2.5）需要更复杂的
  示例才能验证。
- resolve 段的槽位顺序是 `unordered_map` 的哈希序，结果正确但跨运行不确定。barrier 数组顺序
  会跟着变——不影响正确性（barrier 之间无序），但调试时看到的顺序不稳定。
- `AttachmentDescriptionInfo` 没有 `getStencilLoadOp` / `getStencilStoreOp`。§3.5.3 的两个
  discard 判定要按 aspect 检查 stencil 一侧，得先补这两个 getter。
- `setStencilLoadStoreOp` 设的值 `DynamicRender` 读不到——`vk::RenderingAttachmentInfo` 只有
  一组 loadOp / storeOp，depth 与 stencil 是两个独立的 attachment info。当前 `create()` 把
  depth 那份的 op 填给了 ds attachment，stencil 侧的设定被静默丢弃。要么补
  `m_stencil_attachment`，要么在设了不一致的 op 时断言。
- `DynamicRender::begin()` 写 barrier 数组，所以实例不能跨录制线程共用（§3.7）。现有的
  `m_color_attachments` 回填已经有同样的约束，这里只是又多一处。
- `AccessScope` 的字段公开，`AttachmentTransitionInfo` 的字段 private 走 getter/setter。同一个
  头文件里两种风格。`AccessScope` 是纯数据对（要支持 `{eBlit, eTransferRead}` 这种就地写法），
  `AttachmentTransitionInfo` 的 getter 带 `Or(fallback)` 语义，不是裸转发，所以没统一。

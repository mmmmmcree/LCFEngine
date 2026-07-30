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

### 2.2 一个意图携带四项属性

意图不是只为了算 layout。每个值挂四项：

| 属性 | 用途 |
|---|---|
| `specific_layout` | 常规路径的 `vk::ImageLayout` |
| `unified_layout` | 开了 unified layouts 时的 `vk::ImageLayout` |
| `required_image_usage` | 校验 image 建的时候有没有对应的 `vk::ImageUsageFlagBits` |
| `access_scope` | barrier 的 `stage_mask` / `access_mask` |

关键点：**`access_scope` 不经过 layout**。unified layouts 开关只换 layout 那一列，同步 mask
一个字节不变。

```
                        /-- specific_layout --\
                       /                       >-- resolve_layout() --> vk::ImageLayout
AttachmentUsage --表--<---- unified_layout ---/
                       \-- access_scope ---------> (stage mask, access mask)
                        \-- required_image_usage -> create() 时校验
```

四项属性放**一张表**，一个枚举值一行。加枚举值时表长的 `static_assert` 会失败，而不是某一项
静默返回默认值。

「枚举值 → 一行属性」这个形状在 `utilities/core/include/enums/` 下还没有对应设施
（`enum_mapping_traits` 是枚举 1:1、`enum_flags_cast` 要求 flag 类型、
`enum_value_type_mapping_traits` 转的是类型），所以补一个通用的 `enum_attribute_traits`。

### 2.3 三层归属

五个转换字段（entry / in-pass / exit 三个意图 + 两个 scope override）全部归 render info。

| 层 | 决定 | 转换相关的新增 |
|---|---|---|
| `AttachmentSetInfo` | 有哪些 attachment、format / sample / load-store op | **无** |
| `StaticRenderInfo` / `DynamicRenderInfo` | 这条 pass 怎么用它们 | `m_transitions`（slot indexed）+ `m_unified_enabled` |
| `RenderTarget` | 具体是哪张 image | `viewAttachments()` |

依据是份数：

- 一份 `AttachmentSetInfo` 会被多条 pass 用。同一张 depth 图在 geometry pass 里是
  `eDepthStencilAttachment`、在后续 pass 里是 `eDepthStencilReadOnly`——entry / exit 更是
  逐 pass 不同。放进 set，第二条 pass 无处可写。
- `RenderTarget` 是 per-frame 复数（005 里两个实例共享一份声明）。放进去要复制 N 份并维护
  一致性，白拿一个同步不变量。
- `DynamicRender::begin()` 需要的 `vk::Image` 与 `subresourceRange` 从 `viewAttachments()`
  读即可。读资源不等于持有声明。

结论：`AttachmentSetInfo` 与它的 builder **完全不动**，005 的 attachment set 段一行不改。

### 2.4 unified layouts 开关的位置

`m_unified_enabled` 是 render info 上的一个 bool，由调用方 `enableUnifiedLayouts()` 打开。
不引入 policy 类，也不进 `DeviceContext`：

- 消费点只有两个——`DynamicRender::create()` 烘 barrier、`StaticRender::create()` 烘 render
  pass，都在 render info 手上，没有第三方要读。
- 进 `DeviceContext` 意味着声明侧要持有一个指向 device 的 policy 指针，把 attachment / render
  声明的生命周期绑到 device 上。它们现在是纯值类型。
- 一个 bool 包成类，只为 `resolve()` 那一次三元选择。free function 就够：
  `resolve_layout(usage, unified_enabled)`。

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

| 文件 | 动作 |
|---|---|
| `utilities/core/include/enums/enum_attribute_traits.h` | 新增，通用设施 |
| `vk_core/include/vk_core/utils/AccessScope.h` | 新增 |
| `vk_core/include/vk_core/pipeline/graphics/enums.h` | 新增：`AttachmentUsage` + 属性表 + `resolve_layout` |
| `vk_core/include/vk_core/pipeline/graphics/info_structs.h` | `AttachmentTransitionInfo` + 两个 render info 各加 setter 与成员 |
| `vk_core/include/vk_core/pipeline/graphics/RenderTarget.h` | 加 `viewAttachments()` |
| `vk_core/include/vk_core/pipeline/graphics/DynamicRender.h` / `.cpp` | 加 barrier 成员与烘制 |
| `vk_core/include/vk_core/pipeline/graphics/entry.h` / `DynamicRender.cpp` | 加 `register_unified_image_layouts` |
| `vk_core/src/pipeline/graphics/StaticRender.cpp` | `create()` 消费意图 |
| `examples/.../005_hello_static_pipeline_main.cpp` | 删 28 行 barrier，加 2 行声明 |

`AttachmentSetInfo` / `AttachmentSetInfoBuilder` / `RenderTargetInfo` 不动。

### 3.2 `enum_attribute_traits.h`

```cpp
#pragma once

#include "concepts/enum_concept.h"
#include <utility>

namespace lcf {
    //- specialize with a `rows` array indexed by std::to_underlying(Enum); the specialization
    //- site should static_assert std::size(rows) == enum_count_v<Enum>
    template <enum_c Enum>
    struct enum_attribute_traits;

namespace details {
    template <typename T>
    concept has_attribute_rows_c = requires { T::rows; };
}

    template <enum_c Enum>
    requires details::has_attribute_rows_c<enum_attribute_traits<Enum>>
    constexpr const auto & enum_attributes_of(Enum value) noexcept
    {
        return enum_attribute_traits<Enum>::rows[std::to_underlying(value)];
    }
}
```

primary template 只声明不定义——没特化就编译失败。`requires` 卡在访问器上，「没特化」与
「特化了但成员名拼错」两种错误都指向调用点。行序必须与枚举声明序一致，由特化点的
`static_assert` 与注释固定。`rows` 用名词成员，与 `enum_mapping_traits::mappings` 一致；
访问器是自由函数走 `snake_case`。

### 3.3 `AccessScope.h`

```cpp
#pragma once

#include <vulkan/vulkan.hpp>

namespace lcf::vkc {

struct AccessScope
{
    vk::PipelineStageFlags2 stage_mask;
    vk::AccessFlags2 access_mask;
};

} // namespace lcf::vkc
```

独立于 `AttachmentUsage`——barrier 的 override 参数（§3.5）也用它。

### 3.4 `pipeline/graphics/enums.h`

```cpp
#pragma once

#include "vk_core/utils/AccessScope.h"
#include "enums/enum_attribute_traits.h"
#include "enums/enum_count.h"
#include <vulkan/vulkan.hpp>

namespace lcf::vkc {

enum class AttachmentUsage : uint8_t
{
    eNone,
    eDiscard,
    eColorAttachment,
    eDepthStencilAttachment,
    eDepthStencilReadOnly,
    eTransferSource,
    eTransferDestination,
    eShaderRead,
    ePresent,
};

} // namespace lcf::vkc

template <>
struct lcf::enum_attribute_traits<lcf::vkc::AttachmentUsage>
{
    struct Row
    {
        vk::ImageLayout specific_layout;
        vk::ImageLayout unified_layout;
        vk::ImageUsageFlags required_image_usage;
        vkc::AccessScope access_scope;
    };
    //- row order must match the AttachmentUsage declaration order
    static constexpr Row rows[] = {
        { //- eNone
            vk::ImageLayout::eUndefined, vk::ImageLayout::eUndefined, {}, {},
        },
        { //- eDiscard
            vk::ImageLayout::eUndefined, vk::ImageLayout::eUndefined, {},
            { vk::PipelineStageFlagBits2::eAllCommands, vk::AccessFlagBits2::eNone },
        },
        { //- eColorAttachment
            vk::ImageLayout::eColorAttachmentOptimal, vk::ImageLayout::eGeneral,
            vk::ImageUsageFlagBits::eColorAttachment,
            { vk::PipelineStageFlagBits2::eColorAttachmentOutput,
              vk::AccessFlagBits2::eColorAttachmentWrite | vk::AccessFlagBits2::eColorAttachmentRead },
        },
        { //- eDepthStencilAttachment
            vk::ImageLayout::eDepthStencilAttachmentOptimal, vk::ImageLayout::eGeneral,
            vk::ImageUsageFlagBits::eDepthStencilAttachment,
            { vk::PipelineStageFlagBits2::eEarlyFragmentTests | vk::PipelineStageFlagBits2::eLateFragmentTests,
              vk::AccessFlagBits2::eDepthStencilAttachmentWrite | vk::AccessFlagBits2::eDepthStencilAttachmentRead },
        },
        { //- eDepthStencilReadOnly
            vk::ImageLayout::eDepthStencilReadOnlyOptimal, vk::ImageLayout::eGeneral,
            vk::ImageUsageFlagBits::eDepthStencilAttachment,
            { vk::PipelineStageFlagBits2::eEarlyFragmentTests | vk::PipelineStageFlagBits2::eLateFragmentTests |
                  vk::PipelineStageFlagBits2::eFragmentShader,
              vk::AccessFlagBits2::eDepthStencilAttachmentRead },
        },
        { //- eTransferSource
            vk::ImageLayout::eTransferSrcOptimal, vk::ImageLayout::eGeneral,
            vk::ImageUsageFlagBits::eTransferSrc,
            { vk::PipelineStageFlagBits2::eAllTransfer, vk::AccessFlagBits2::eTransferRead },
        },
        { //- eTransferDestination
            vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::eGeneral,
            vk::ImageUsageFlagBits::eTransferDst,
            { vk::PipelineStageFlagBits2::eAllTransfer, vk::AccessFlagBits2::eTransferWrite },
        },
        { //- eShaderRead
            vk::ImageLayout::eShaderReadOnlyOptimal, vk::ImageLayout::eGeneral,
            vk::ImageUsageFlagBits::eSampled,
            { vk::PipelineStageFlagBits2::eAllGraphics | vk::PipelineStageFlagBits2::eComputeShader,
              vk::AccessFlagBits2::eShaderSampledRead },
        },
        { //- ePresent: synchronized by semaphores; the barrier only carries the layout
            vk::ImageLayout::ePresentSrcKHR, vk::ImageLayout::ePresentSrcKHR, {}, {},
        },
    };
};

static_assert(std::size(lcf::enum_attribute_traits<lcf::vkc::AttachmentUsage>::rows) ==
    lcf::enum_count_v<lcf::vkc::AttachmentUsage>);

namespace lcf::vkc {

constexpr vk::ImageLayout resolve_layout(AttachmentUsage usage, bool unified_enabled) noexcept
{
    const auto & row = enum_attributes_of(usage);
    return unified_enabled ? row.unified_layout : row.specific_layout;
}

} // namespace lcf::vkc
```

`eUndefined` 与 `ePresentSrcKHR` 两列相同，不是可选的：`eUndefined` 承载「内容可丢弃 +
初始化 metadata」语义，规范要求保留；`ePresentSrcKHR` 不在扩展覆盖范围（§6）。

`access_scope` 三处保守，都可以 override（§3.5）：`eTransferSource/Destination` 用
`eAllTransfer` 而非 `eBlit`/`eCopy`/`eResolve`（意图不区分 transfer 子类）；`eShaderRead` 的
stage 是并集；`ePresent` 全空。`eDiscard` 的 `eAllCommands` 是入口方向的保守默认——丢弃意味着
不关心之前谁写的，等所有前序命令是唯一安全解。

`required_image_usage` 在 `ePresent` 那行为空：swapchain image 的 usage 位由 swapchain 决定。

### 3.5 `info_structs.h`

`AttachmentTransitionInfo` 接在 `details::attachment_key_c` 之后、两个 render info 之前：

```cpp
struct AttachmentTransitionInfo
{
    //- eNone on entry_usage means "derive from loadOp at bake time"
    AttachmentUsage entry_usage = AttachmentUsage::eNone;
    AttachmentUsage in_pass_usage = AttachmentUsage::eColorAttachment;
    AttachmentUsage exit_usage = AttachmentUsage::eNone;
    std::optional<AccessScope> entry_src_scope_opt;
    std::optional<AccessScope> exit_dst_scope_opt;
    std::optional<vk::ImageLayout> in_pass_layout_opt; //- escape hatch, bypasses in_pass_usage
};
```

`entry_usage` 的 `eNone` 是「未声明」哨兵——烘制时读 `loadOp` 决定，所以 `setLoadStoreOp` 与
`setEntryUsage` 之间没有隐式耦合，调用顺序无关。`exit_usage` 的 `eNone` 是「不发 barrier」。
两者同名不同义，但都在各自字段上唯一，不会歧义。

两个 render info 各加同一组接口：

```cpp
    Self & enableUnifiedLayouts() noexcept { m_unified_enabled = true; return *this; }
    //- entry defaults from loadOp; exit defaults to eNone (no barrier emitted)
    Self & setEntryUsage(details::attachment_key_c auto key, AttachmentUsage usage) noexcept;
    Self & setExitUsage(details::attachment_key_c auto key, AttachmentUsage usage) noexcept;
    Self & setInPassUsage(details::attachment_key_c auto key, AttachmentUsage usage) noexcept;
    //- overrides the derived scope; mandatory when setLayout bypasses in_pass_usage
    Self & setEntrySrcScope(details::attachment_key_c auto key, const AccessScope & scope) noexcept;
    Self & setExitDstScope(details::attachment_key_c auto key, const AccessScope & scope) noexcept;
    Self & setLayout(details::attachment_key_c auto key, vk::ImageLayout layout) noexcept;
private:
    std::vector<AttachmentTransitionInfo> m_transitions; //- slot indexed
    bool m_unified_enabled = false;
```

构造时按 slot 段填 `in_pass_usage` 默认值：color 段与 resolve 段 `eColorAttachment`，
ds slot `eDepthStencilAttachment`。`DynamicRenderInfo` 现有的 `m_layouts` 数组删掉——in-pass
layout 改为烘制时算，`vk::RenderingAttachmentInfo::imageLayout` 与 barrier 的 `newLayout`
用同一个值，不会写歪。

`DynamicRenderInfo` 只加一个成员：

```cpp
    bool m_by_region_enabled = false;  //- via enableByRegionDependency()
```

`eByRegion` 只加在入口 barrier 上——出口的消费者（blit / transfer）通常在 framebuffer 空间
之外。只在所有前序 producer 写的是同一像素坐标时成立，所以是 opt-in。

**`StaticRenderInfo` 的消费方式**：`StaticRender::create()` 把 entry / exit 意图过
`resolve_layout` 写进 `AttachmentDescriptionInfo` 的 `initialLayout` / `finalLayout`，
`in_pass_usage` 成为 `AttachmentReferenceInfo` 的 layout。mask 保持隐式（驱动补 external
dependency）。

与现有 `setInitialFinalLayout` 的优先级：**该 slot 声明了 entry / exit 意图时以意图为准**，
未声明的 slot 保留手写 layout。按声明存在性判定，不按调用顺序。

### 3.6 `RenderTarget.h`

```cpp
    std::span<const Attachment> viewAttachments() const noexcept { return m_attachments; }
```

`Attachment` 是 move-only，但 `span<const Attachment>` 只读不拷。`getImage()` 返回
`const Image &`，`Image` 有 `operator vk::Image()`；`getDescription().getSubresourceRange()`
给 aspect / mip / layer。

### 3.7 `DynamicRender` 的 barrier

新增成员：

```cpp
    struct BarrierSlotMap
    {
        uint32_t slot = vk::AttachmentUnused;
        uint32_t entry_index = vk::AttachmentUnused;  //- index into m_entry_barriers
        uint32_t exit_index = vk::AttachmentUnused;   //- index into m_exit_barriers
    };
    using BarrierList = std::vector<vk::ImageMemoryBarrier2>;
private:
    BarrierList m_entry_barriers;
    BarrierList m_exit_barriers;
    std::vector<BarrierSlotMap> m_barrier_slot_maps; //- one per slot needing any barrier
    vk::DependencyFlags m_dependency_flags;
```

入口与出口必须是两个数组：`pipelineBarrier2` 要求 `VkImageMemoryBarrier2` 连续，而两者是两次
调用。映射表只需一张——同一 slot 的两条 barrier 共用 image 与 range，一次循环回填两边。

`create()` 里每个 slot：

```cpp
const AttachmentTransitionInfo & transition = render_info.m_transitions[slot];
bool unified = render_info.m_unified_enabled;
vk::ImageLayout in_pass_layout = transition.in_pass_layout_opt
    .value_or(resolve_layout(transition.in_pass_usage, unified));
AccessScope in_pass_scope = enum_attributes_of(transition.in_pass_usage).access_scope;
//- eNone means "derive from loadOp"
AttachmentUsage entry_usage = transition.entry_usage != AttachmentUsage::eNone
    ? transition.entry_usage
    : (discards_on_load(descriptions[slot], is_depth_stencil_slot)
        ? AttachmentUsage::eDiscard
        : transition.in_pass_usage);
BarrierSlotMap slot_map;
slot_map.slot = slot;

vk::ImageLayout entry_layout = resolve_layout(entry_usage, unified);
AccessScope entry_src = transition.entry_src_scope_opt
    .value_or(enum_attributes_of(entry_usage).access_scope);
if (entry_layout != in_pass_layout or entry_src.stage_mask) {
    slot_map.entry_index = static_cast<uint32_t>(m_entry_barriers.size());
    m_entry_barriers.emplace_back()
        .setOldLayout(entry_layout)
        .setNewLayout(in_pass_layout)
        .setSrcStageMask(entry_src.stage_mask)
        .setSrcAccessMask(entry_src.access_mask)
        .setDstStageMask(in_pass_scope.stage_mask)
        .setDstAccessMask(in_pass_scope.access_mask);
}

if (transition.exit_usage != AttachmentUsage::eNone) {
    AccessScope exit_dst = transition.exit_dst_scope_opt
        .value_or(enum_attributes_of(transition.exit_usage).access_scope);
    slot_map.exit_index = static_cast<uint32_t>(m_exit_barriers.size());
    m_exit_barriers.emplace_back()
        .setOldLayout(discards_on_store(descriptions[slot], is_depth_stencil_slot)
            ? vk::ImageLayout::eUndefined : in_pass_layout)
        .setNewLayout(resolve_layout(transition.exit_usage, unified))
        .setSrcStageMask(in_pass_scope.stage_mask)
        .setSrcAccessMask(in_pass_scope.access_mask)
        .setDstStageMask(exit_dst.stage_mask)
        .setDstAccessMask(exit_dst.access_mask);
}
if (slot_map.entry_index != vk::AttachmentUnused or slot_map.exit_index != vk::AttachmentUnused) {
    m_barrier_slot_maps.emplace_back(slot_map);
}
```

两个 discard 判定（文件级匿名 namespace）：

```cpp
bool discards_on_load(const AttachmentDescriptionInfo & desc, bool is_depth_stencil) noexcept;
bool discards_on_store(const AttachmentDescriptionInfo & desc, bool is_depth_stencil) noexcept;
```

`is_depth_stencil` 为真时要 `loadOp` 与 `stencilLoadOp`（`storeOp` 与 `stencilStoreOp`）两侧
都是 discard 类才返回真。

队列族索引不设，默认 `vk::QueueFamilyIgnored`——同队列无所有权转移，与 `Swapchain.cpp` 一致。

`begin()` / `end()`：

```cpp
void DynamicRender::begin(CommandBufferProxy & cmd, const RenderTarget & render_target) noexcept
{
    auto attachments = render_target.viewAttachments();
    //- the exit barriers reference the same attachments, so fill both sides here and end()
    //- needs no render target parameter
    for (const auto & [slot, entry_index, exit_index] : m_barrier_slot_maps) {
        vk::Image image = attachments[slot].getImage();
        vk::ImageSubresourceRange range = attachments[slot].getDescription().getSubresourceRange();
        if (entry_index != vk::AttachmentUnused) {
            m_entry_barriers[entry_index].setImage(image).setSubresourceRange(range);
        }
        if (exit_index != vk::AttachmentUnused) {
            m_exit_barriers[exit_index].setImage(image).setSubresourceRange(range);
        }
    }
    if (not m_entry_barriers.empty()) {
        vk::DependencyInfo dependency_info;
        dependency_info.setDependencyFlags(m_dependency_flags)
            .setImageMemoryBarriers(m_entry_barriers);
        cmd.pipelineBarrier2(dependency_info);
    }
    //- ... existing per-frame attachment info fill + beginRendering
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

`vk::DependencyInfo` 在栈上构造，不做成员——做成员的话 `pImageMemoryBarriers` 指向
`m_exit_barriers` 的缓冲区，`DynamicRender` 一旦被 move 就悬空。每帧一次
`setImageMemoryBarriers` 的成本可忽略，与 `m_rendering` 现有的 `setColorAttachments` 同一
处理方式。

同一处校验 usage 位：`entry_usage` / `in_pass_usage` / `exit_usage` 三行的
`required_image_usage` 并集必须被 `Image::getDescription().getUsageFlags()` 覆盖。`create()`
拿不到 `RenderTarget`，所以断言落在 `begin()` 首次调用（§7）。

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

attachment set / render target / image 段（`:167-207`）不动。声明段两条路径逐字相同：

```cpp
    static_render_info.setLoadStoreOp(color_key, vk::AttachmentLoadOp::eClear, vk::AttachmentStoreOp::eStore)
        .setExitUsage(color_key, vkc::AttachmentUsage::eTransferSource)
        .setExitDstScope(color_key, {vk::PipelineStageFlagBits2::eBlit, vk::AccessFlagBits2::eTransferRead})
        .addSubpass(std::move(subpass_info));
```

```cpp
    dynamic_render_info.setLoadStoreOp(color_key, vk::AttachmentLoadOp::eClear, vk::AttachmentStoreOp::eStore)
        .setExitUsage(color_key, vkc::AttachmentUsage::eTransferSource)
        .setExitDstScope(color_key, {vk::PipelineStageFlagBits2::eBlit, vk::AccessFlagBits2::eTransferRead});
```

入口不写——`loadOp = eClear` 推出 `eDiscard`。`setExitDstScope` 那行可省，省了就是
`eAllTransfer`，比 `eBlit` 保守一档。

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
- 要拿收益必须主动全线改 `eGeneral`。`resolve_layout()` 就是那个「主动改」的落地点。

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
按上面是 UB。这四处要改走 `resolve_layout` 或由调用方传入，才能开 unified。在此之前
`register_unified_image_layouts` 不接线。

## 7. 范围外

- **跨 pass barrier 合并。** pass A 的出口与 pass B 的入口是同一转换时应合成一条。需要
  RenderGraph 的 pass 依赖图。
- **subresource 收窄。** 现在整个 attachment 的 range 一起转换。
- **`eByRegion` 自动判定。** 需要知道前序 producer 是否 framebuffer-local。
- **静态路径的显式 external dependency。** `m_transitions` 里的 scope 信息已经现成，可以直接
  喂 `SubpassDependencyInfo`，但要决定与多 subpass 的交互，另开设计。
- **queue family ownership transfer。** 现在全部 `eQueueFamilyIgnored`。
- **`resolve_layout` 扩到非 attachment image。** 采样纹理、storage image 的 layout 也该过同一
  个函数，否则 unified 只覆盖一半。等纹理路径落地后再做。
- **usage 位校验时机。** §3.7 那条断言现在落在 `begin()` 首次调用。想在 `create()` 时报，
  需要多传一个 usage flags 列表，或让 `RenderTargetInfo` 记下每个 slot 的 usage 位。
- **`enum_attribute_traits` 复用到 `DescriptorSetIndex`。** 那套 `[strategy:4][index:4]` 编码
  载荷小、是笛卡尔积，位编码在那里可能仍是对的，只共享「decode 函数散开」这个较弱的问题。
  等描述符重构那条线动到时再评估。

## 8. 落地顺序

1. `enum_attribute_traits.h` —— 纯 std，无新依赖，可单独编过（primary template 不定义，
   不特化就不实例化）。
2. `AccessScope.h` + `pipeline/graphics/enums.h`（枚举 + 属性表 + `static_assert` +
   `resolve_layout`）—— 只依赖 1 与 vulkan.hpp。
3. `RenderTarget::viewAttachments()`。
4. `AttachmentTransitionInfo` + `StaticRenderInfo` 的 setter 与成员 +
   `StaticRender::create()` 消费意图 —— **静态路径先接是动态路径的安全网**：静态路径有驱动的
   隐式 barrier 兜底，属性表填错会体现在 layout 上而不是同步上，更容易定位。005 静态路径跑通、
   画面不变即验证通过。
5. `DynamicRenderInfo` 的 setter 与成员（删 `m_layouts`）+ `DynamicRender` 烘 barrier /
   `begin` / `end`。
6. 005 删 28 行 barrier，动态路径跑通。开 validation layer，重点看
   `VUID-vkCmdBeginRendering-*`、`VUID-VkImageMemoryBarrier2-oldLayout-*` 与 sync validation
   的 hazard 报告。
7. 可延后：`Swapchain` 四处 layout 改走 `resolve_layout`（§6），然后
   `register_unified_image_layouts` 接线。需要支持的驱动才好验证。

`libs/vk_core/CMakeLists.txt` 用 `GLOB_RECURSE`，新增 .cpp 不改 CMake，但要重跑 configure。

## 9. 已知遗留

- `RenderTarget` 缺 `setResolveAttachment` / `setDepthStencilAttachment`，所以 resolve 段与
  ds slot 的 barrier 路径在 005 里跑不到，只有单 color 被覆盖。批量提交（§2.5）需要更复杂的
  示例才能验证。
- resolve 段的槽位顺序是 `unordered_map` 的哈希序，结果正确但跨运行不确定。barrier 数组顺序
  会跟着变——不影响正确性（barrier 之间无序），但调试时看到的顺序不稳定。

# 动态渲染 layout 转换：实现步骤

设计见 `vkc-dynamic-render-layout-transition-design.md`。本文只覆盖**让 005 的动态路径收敛成四行**
所需的改动：

```cpp
    dynamic_render.begin(cmd, render_target);
    dynamic_graphics_pipeline.bind(cmd);
    cmd.draw(3, 1, 0, 0);
    dynamic_render.end(cmd);
```

静态路径（`StaticRenderInfo` 的意图 setter、`makeAttachmentReference` 转发、
`StaticRender::create()` 消费意图）不在范围内——005 里静态那四行已经注释掉了，接静态路径对这个
目标没有贡献。设计文档 §3.5.5、§3.6 那部分留到下一轮。

## 0. 当前状态

已经落地的（不用再写）：

- `utilities/enums/include/enums/enum_attributes_traits.h`：`enum_common_attributes_traits` +
  `enum_specialized_attributes_traits` 主模板。
- `libs/vk_core/include/vk_core/pipeline/graphics/enums.h`：`AttachmentUsage` 九个值 +
  九行属性表 + 四个编译期访问器。
- `info_structs.h`：`AccessScope`、`AttachmentTransitionInfo`（含全部 setter/getter）、
  `AttachmentSetInfo::makeDefaultTransitions()` 声明、`DynamicRenderInfo` 的七个意图 setter +
  `m_transitions`（构造函数里已从 `makeDefaultTransitions()` 取初值）+ `m_unified_layout_enabled`。
- `info_structs.cpp`：`AttachmentSetInfo::makeDefaultTransitions()` 定义。

**注意：当前树编译不过。** `DynamicRender.cpp:33` 调 `render_info.getLayouts()`，而
`DynamicRenderInfo` 已经没有 `m_layouts` / `getLayouts()` 了。第 1–5 步都是纯新增，编不过的状态
会一直持续到第 6 步改完 `DynamicRender.cpp` 才恢复。所以第 6 步之前不必尝试整体构建，只能靠单
文件语法检查。

## 1. `utilities/enums/include/enums/enum_attributes_traits.h`

一行改动，把 `name_of_v` 收成 constexpr（`enum_name` 现在是 constexpr）：

```diff
     template <Enum enum_value>
-    inline static auto name_of_v = enum_name<Enum>(enum_value);
+    inline static constexpr auto name_of_v = enum_name<Enum>(enum_value);
```

与本功能无关，但改的是同一个文件，顺手。若 `enum_name` 那边实际不是 constexpr 就跳过这步。

## 2. `libs/vk_core/include/vk_core/pipeline/graphics/enums.h`

### 2.1 加 include

```diff
 #include "enums/enum_attributes_traits.h"
+#include "enums/enum_count.h"
 #include <vulkan/vulkan.hpp>
```

### 2.2 表后面加 `static_assert`

紧跟 `attributes_list` 的右花括号，仍在 private 区（表是 private，类外访问不到）：

```cpp
    static_assert(std::size(attributes_list) == lcf::enum_count_v<lcf::vkc::AttachmentUsage>,
        "attributes_list must have exactly one row per AttachmentUsage value, in declaration order");
```

### 2.3 `public:` 区整段替换

现在四个访问器是非静态模板。改成「运行期版做实现 + 编译期版转发」，并全部加 `static`：

```cpp
public:
    //- run-time form: usage comes from AttachmentTransitionInfo at bake time
    static constexpr vk::ImageLayout layout_of(lcf::vkc::AttachmentUsage usage, bool unified_enabled = false) noexcept
    {
        const auto & attributes = get_attributes(usage);
        return unified_enabled ? attributes.unified_layout : attributes.specific_layout;
    }
    static constexpr vk::ImageUsageFlags required_image_usage_of(lcf::vkc::AttachmentUsage usage) noexcept
    {
        return get_attributes(usage).required_image_usage;
    }
    static constexpr vk::PipelineStageFlags2 stage_mask_of(lcf::vkc::AttachmentUsage usage) noexcept
    {
        return get_attributes(usage).stage_mask;
    }
    static constexpr vk::AccessFlags2 access_mask_of(lcf::vkc::AttachmentUsage usage) noexcept
    {
        return get_attributes(usage).access_mask;
    }
    //- compile-time form: usage known at the call site, forwards to the run-time one
    template <lcf::vkc::AttachmentUsage usage>
    static constexpr vk::ImageLayout layout_of(bool unified_enabled = false) noexcept { return layout_of(usage, unified_enabled); }
    template <lcf::vkc::AttachmentUsage usage>
    static constexpr vk::ImageUsageFlags required_image_usage_of() noexcept { return required_image_usage_of(usage); }
    template <lcf::vkc::AttachmentUsage usage>
    static constexpr vk::PipelineStageFlags2 stage_mask_of() noexcept { return stage_mask_of(usage); }
    template <lcf::vkc::AttachmentUsage usage>
    static constexpr vk::AccessFlags2 access_mask_of() noexcept { return access_mask_of(usage); }
```

两组不歧义：`AttachmentUsage` 是 scoped enum，不隐式转 `bool`，`layout_of(usage, unified)` 只
匹配运行期版；`layout_of<usage>()` 要显式模板实参，只匹配模板版。

### 2.4 文件末尾加两个包装

特化之后重开 `lcf::vkc`：

```cpp
namespace lcf::vkc {

constexpr vk::ImageLayout to_image_layout(AttachmentUsage usage, bool unified_enabled = false) noexcept
{
    return lcf::enum_specialized_attributes_traits<AttachmentUsage>::layout_of(usage, unified_enabled);
}

constexpr vk::ImageUsageFlags to_required_image_usage(AttachmentUsage usage) noexcept
{
    return lcf::enum_specialized_attributes_traits<AttachmentUsage>::required_image_usage_of(usage);
}

} // namespace lcf::vkc
```

不叫 `resolve_layout`：这个模块里 "resolve" 已经归 MSAA resolve attachment。

**验证**：这一步的产物可以单独编——写个 TU 只 include `enums.h`，加
`static_assert(to_image_layout(AttachmentUsage::eColorAttachment) == vk::ImageLayout::eColorAttachmentOptimal);`
再删掉。这是全流程唯一能在编译期完整验证的一段，值得做。

## 3. `libs/vk_core/include/vk_core/pipeline/graphics/info_structs.h`

### 3.1 `AccessScope` 之后加 `to_access_scope`

```cpp
inline AccessScope to_access_scope(AttachmentUsage usage) noexcept
{
    using Traits = lcf::enum_specialized_attributes_traits<AttachmentUsage>;
    return { Traits::stage_mask_of(usage), Traits::access_mask_of(usage) };
}
```

放这儿而不是 `enums.h`：返回类型 `AccessScope` 定义在本文件，`enums.h` 不该反向依赖。

### 3.2 `AttachmentTransitionInfo` 之后加烘制中间体

```cpp
struct ResolvedTransition
{
    vk::ImageLayout entry_layout = vk::ImageLayout::eUndefined;
    vk::ImageLayout in_pass_layout = vk::ImageLayout::eUndefined;
    vk::ImageLayout exit_layout = vk::ImageLayout::eUndefined;
    vk::ImageLayout exit_old_layout = vk::ImageLayout::eUndefined;
    AccessScope entry_src_scope;
    AccessScope in_pass_scope;
    AccessScope exit_dst_scope;
    vk::ImageUsageFlags required_image_usage;
    bool entry_barrier_needed = false;
    bool exit_barrier_needed = false;
};

//- turns one slot's declared intents plus its load/store ops into concrete layouts and masks
ResolvedTransition resolve_transition(
    const AttachmentDescriptionInfo & description,
    const AttachmentTransitionInfo & transition,
    bool unified_enabled) noexcept;
```

`ResolvedTransition` 必须是完整类型可见的——`DynamicRender.h` 的 `bakeBarriers` 要
`std::span<const ResolvedTransition>`，span 的元素类型不能是 incomplete。

### 3.3 `DynamicRenderInfo` 加 `eByRegion` 开关

`enableUnifiedLayouts()` 旁边：

```cpp
    Self & enableByRegionDependency() noexcept { m_by_region_enabled = true; return *this; }
```

私有成员区：

```cpp
    bool m_by_region_enabled = false;
```

**不加 `getTransitions()` / `isUnifiedLayoutEnabled()` / `isByRegionEnabled()`。** 设计文档 §3.7
写了这三个私有读取口，但 `DynamicRender` 已经是 `DynamicRenderInfo` 的友元，且现有代码已经在
直接读 `render_info.m_rendering`。为三个字段加三个转发不如直接读成员，与既有风格一致。

## 4. `libs/vk_core/src/pipeline/graphics/info_structs.cpp`

### 4.1 加 include

```diff
 #include "vk_core/pipeline/graphics/info_structs.h"
+#include "vk_core/utils/format_utils.h"
 #include <cassert>
 #include <atomic>
```

### 4.2 两个 discard 判定

文件顶部那个匿名 namespace（现在只有 `next_attachment_set_id()` 的声明）里加声明。它在
`namespace lcf::vkc` 之前，所以参数类型要写全名：

```cpp
bool discards_on_load(const lcf::vkc::AttachmentDescriptionInfo & description) noexcept;
bool discards_on_store(const lcf::vkc::AttachmentDescriptionInfo & description) noexcept;
```

声明在前是必须的——`resolve_transition` 在 `namespace lcf::vkc` 里调它们，而定义在文件末尾。

定义放文件末尾那个已有的匿名 namespace 实现块（`next_attachment_set_id` 定义处）。两个匿名
namespace 块指的是同一个 namespace，所以声明与定义能对上：

```cpp
bool discards_on_load(const lcf::vkc::AttachmentDescriptionInfo & description) noexcept
{
    constexpr auto is_discard = [](vk::AttachmentLoadOp op) noexcept {
        return op == vk::AttachmentLoadOp::eClear or op == vk::AttachmentLoadOp::eDontCare;
    };
    vk::Format format = description.getFormat();
    bool has_depth = lcf::vkc::utils::is_depth_format(format);
    bool has_stencil = lcf::vkc::utils::is_stencil_format(format);
    if (not has_depth and not has_stencil) { return is_discard(description.getLoadOp()); }
    //- only downgrade when every aspect the format actually has is discarded; Vulkan ignores
    //- stencilLoadOp on a depth-only format, so its eLoad default must not veto the downgrade
    return (not has_depth or is_discard(description.getLoadOp())) and
        (not has_stencil or is_discard(description.getStencilLoadOp()));
}

bool discards_on_store(const lcf::vkc::AttachmentDescriptionInfo & description) noexcept
{
    //- eNone is not a discard: contents are either preserved or undefined, and treating
    //- "maybe preserved" as discardable would throw away data the caller may still want
    constexpr auto is_discard = [](vk::AttachmentStoreOp op) noexcept {
        return op == vk::AttachmentStoreOp::eDontCare;
    };
    vk::Format format = description.getFormat();
    bool has_depth = lcf::vkc::utils::is_depth_format(format);
    bool has_stencil = lcf::vkc::utils::is_stencil_format(format);
    if (not has_depth and not has_stencil) { return is_discard(description.getStoreOp()); }
    return (not has_depth or is_discard(description.getStoreOp())) and
        (not has_stencil or is_discard(description.getStencilStoreOp()));
}
```

按 format 判 aspect 而不是让调用方传 `is_depth_stencil` 布尔：format 已经在 description 里，多
一个参数就多一处能传错的地方。

### 4.3 `makeDefaultTransitions()` 之后加 `resolve_transition`

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
    //- the previous frame may have written this image in the same layout (WAW)
    resolved.entry_barrier_needed = resolved.entry_layout != resolved.in_pass_layout or
        static_cast<bool>(resolved.entry_src_scope.m_stage_mask);
    resolved.exit_barrier_needed = exit_usage != AttachmentUsage::eNone;
    return resolved;
}
```

## 5. `libs/vk_core/include/vk_core/pipeline/graphics/RenderTarget.h`

`viewAttachmentImageViews()` 旁边加一个：

```cpp
    std::span<const Attachment> viewAttachments() const noexcept { return m_attachments; }
```

需要 `#include <span>`（当前只 include 了 `<vector>` 与 `<ranges>`）。

barrier 回填要的是 image 与 subresource range，不是 image view，所以现有的
`viewAttachmentImageViews()` 不动——`begin()` 填 `RenderingAttachmentInfo` 还在用它。

## 6. `libs/vk_core/include/vk_core/pipeline/graphics/DynamicRender.h`

### 6.1 换 include，删前置声明

```diff
-#include "vk_core/utils/DynamicStructureChain.h"
+#include "vk_core/utils/DynamicStructureChain.h"
+#include "vk_core/pipeline/graphics/info_structs.h"

 namespace lcf::vkc {

-class DynamicRenderInfo;
 class RenderTarget;
 class CommandBufferProxy;
```

要完整类型是因为 `bakeBarriers` 的 `std::span<const ResolvedTransition>`。无环：
`info_structs.h` 不 include `DynamicRender.h`。

### 6.2 `DynamicRender` 加类型与成员

`using FormatList` 之后：

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
```

`makeScopeInfo()` 之后加私有方法声明，与私有成员区分开：

```cpp
private:
    void bakeBarriers(std::span<const ResolvedTransition> resolved) noexcept;
private:
    utils::DynamicStructureChain<Root> m_rendering;
    ...
    vk::Format m_stencil_format = vk::Format::eUndefined;
    BarrierList m_entry_barriers;
    BarrierList m_exit_barriers;
    BarrierSlotMapList m_barrier_slot_maps;
    vk::DependencyFlags m_dependency_flags;
```

入口与出口必须两个数组：`pipelineBarrier2` 要求 `VkImageMemoryBarrier2` 连续，而两者是两次调用。
映射表一张够——同一 slot 的两条 barrier 共用 image 与 range。

## 7. `libs/vk_core/src/pipeline/graphics/DynamicRender.cpp`

### 7.1 加 include

```diff
 #include "vk_core/utils/format_utils.h"
+#include <cassert>
 #include <ranges>
```

### 7.2 `create()`：换 layout 来源，末尾接烘制

```diff
     m_rendering = render_info.m_rendering;
     const auto & descriptions = render_info.getAttachmentDescriptions();
     const auto & color_resolve_list = render_info.getColorResolveList();
-    const auto & layouts = render_info.getLayouts();
+    const auto & transitions = render_info.m_transitions;
+    bool unified_enabled = render_info.m_unified_layout_enabled;
     uint32_t color_count = render_info.getColorAttachmentCount();
+    uint32_t attachment_count = static_cast<uint32_t>(descriptions.size());
     m_color_attachments.clear();
     m_color_formats.clear();
     m_resolve_indices.clear();
+    m_entry_barriers.clear();
+    m_exit_barriers.clear();
+    m_barrier_slot_maps.clear();
     m_color_attachments.reserve(color_count);
     m_color_formats.reserve(color_count);
     m_resolve_indices.reserve(color_count);
+    m_barrier_slot_maps.reserve(attachment_count);
+    m_dependency_flags = render_info.m_by_region_enabled ? vk::DependencyFlagBits::eByRegion
+                                                        : vk::DependencyFlags {};
+
+    //- resolved once per slot, consumed by both the rendering info and the barriers
+    std::vector<ResolvedTransition> resolved;
+    resolved.reserve(attachment_count);
+    for (uint32_t slot = 0; slot < attachment_count; ++slot) {
+        resolved.emplace_back(resolve_transition(descriptions[slot], transitions[slot], unified_enabled));
+    }
```

循环体里两处 `layouts[...]` 换成 `resolved[...].in_pass_layout`：

```diff
         m_color_attachments.emplace_back()
-            .setImageLayout(layouts[color_index])
+            .setImageLayout(resolved[color_index].in_pass_layout)
             .setResolveMode(resolve_mode)
             .setLoadOp(description.getLoadOp())
             .setStoreOp(description.getStoreOp());
         if (resolve_index != vk::AttachmentUnused) {
-            m_color_attachments.back().setResolveImageLayout(layouts[resolve_index]);
+            m_color_attachments.back().setResolveImageLayout(resolved[resolve_index].in_pass_layout);
         }
```

**最容易漏的一处**：ds 段前的提前返回要变成 `if` 块，因为后面还有 `bakeBarriers`：

```diff
-    if (not render_info.hasDepthStencilAttachment()) { return {}; }
-    const AttachmentDescriptionInfo & description = descriptions.back();
-    vk::RenderingAttachmentInfo depth_stencil_attachment;
-    depth_stencil_attachment.setImageLayout(layouts.back())
-        .setLoadOp(description.getLoadOp())
-        .setStoreOp(description.getStoreOp());
-    if (utils::is_depth_format(description.getFormat())) {
-        m_depth_attachment = depth_stencil_attachment;
-        m_depth_format = description.getFormat();
-    }
-    if (utils::is_stencil_format(description.getFormat())) {
-        m_stencil_attachment = depth_stencil_attachment;
-        m_stencil_format = description.getFormat();
-    }
+    if (render_info.hasDepthStencilAttachment()) {
+        const AttachmentDescriptionInfo & description = descriptions.back();
+        vk::RenderingAttachmentInfo depth_stencil_attachment;
+        depth_stencil_attachment.setImageLayout(resolved.back().in_pass_layout)
+            .setLoadOp(description.getLoadOp())
+            .setStoreOp(description.getStoreOp());
+        if (utils::is_depth_format(description.getFormat())) {
+            m_depth_attachment = depth_stencil_attachment;
+            m_depth_format = description.getFormat();
+        }
+        if (utils::is_stencil_format(description.getFormat())) {
+            m_stencil_attachment = depth_stencil_attachment;
+            m_stencil_format = description.getFormat();
+        }
+    }
+    this->bakeBarriers(resolved);
     return {};
```

漏了这个改动，005 单 color 无 ds，`bakeBarriers` 永远不会被调用，barrier 数组全空，
`begin()` / `end()` 静默什么都不做——不报错，画面也可能凑巧对，最难查。

### 7.3 新增 `bakeBarriers()`

放 `create()` 与 `begin()` 之间：

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

队列族索引不设，默认 `vk::QueueFamilyIgnored`——同队列无所有权转移，与 `Swapchain.cpp` 一致。
`vk::AttachmentUnused`（`~0u`）当哨兵，与合法下标不冲突。

### 7.4 `begin()` 开头插回填与提交

现有函数体第一行 `auto image_views = ...` **之前**：

```cpp
    auto attachments = render_target.viewAttachments();
    //- the exit barriers reference the same attachments, so fill both sides here; end() then
    //- needs no render target parameter
    for (const auto & slot_map : m_barrier_slot_maps) {
        const Attachment & attachment = attachments[slot_map.slot];
        assert((attachment.getImage().getDescription().getUsageFlags() & slot_map.required_image_usage) ==
            slot_map.required_image_usage and
            "attachment image lacks a usage bit required by its declared transition intents");
        vk::Image image = attachment.getImage();
        vk::ImageSubresourceRange range = attachment.getDescription().getSubresourceRange();
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
```

必须在 `beginRendering` 之前——rendering scope 内不能发 image barrier。所以是插在函数最前面，
不是穿插进现有代码。

`vk::DependencyInfo` 在栈上：做成员的话 `pImageMemoryBarriers` 指向 `m_entry_barriers` 的缓冲
区，`DynamicRender` 一旦被 move 就悬空。

### 7.5 `end()` 加出口提交

```cpp
void DynamicRender::end(CommandBufferProxy & cmd) noexcept
{
    cmd.endRendering();
    if (m_exit_barriers.empty()) { return; }
    vk::DependencyInfo dependency_info;
    dependency_info.setImageMemoryBarriers(m_exit_barriers);
    cmd.pipelineBarrier2(dependency_info);
}
```

出口不带 `m_dependency_flags`：`eByRegion` 只对入口有意义，出口的消费者（blit）在 framebuffer
空间之外。

**到这里树应该重新编得过。** 先只跑构建，别急着跑 005。

## 8. `examples/.../005_hello_static_pipeline_main.cpp`

### 8.1 声明段加出口意图（`:242`）

```diff
     vkc::DynamicRenderInfo dynamic_render_info {attachment_set};
-    dynamic_render_info.setLoadStoreOp(color_key, vk::AttachmentLoadOp::eClear, vk::AttachmentStoreOp::eStore);
+    dynamic_render_info.setLoadStoreOp(color_key, vk::AttachmentLoadOp::eClear, vk::AttachmentStoreOp::eStore)
+        .setExitUsage(color_key, vkc::AttachmentUsage::eTransferSource)
+        .setExitDstScope(color_key, {vk::PipelineStageFlagBits2::eBlit, vk::AccessFlagBits2::eTransferRead});
```

**`setExitUsage` 那行是必须的。** 不写就不发出口 barrier，present 的 blit 会在
`eColorAttachmentOptimal` 下读这张 image——UB，validation 会报。

入口不用写：`loadOp = eClear` 让 `resolve_transition` 推出 `eDiscard`，`oldLayout = eUndefined`，
与手写那条一致。

`setExitDstScope` 可省，省了取属性表的 `eAllTransfer | eTransferRead`，比 `eBlit` 保守一档。

### 8.2 渲染循环删 28 行（`:289-318`）

删 `:289-302`（`color_attachment`、`color_range`、`to_color_barrier`、`to_color_dep_info` 及其
`pipelineBarrier2`）与 `:307-318`（`to_transfer_src_barrier` 及其提交）。剩下：

```cpp
            dynamic_render.begin(cmd, render_target);
            dynamic_graphics_pipeline.bind(cmd);
            cmd.draw(3, 1, 0, 0);
            dynamic_render.end(cmd);
```

`color_attachment` / `color_range` 两个局部变量只被删掉的 barrier 用，一起删。`:328` 的
`present_image` 自己从 `render_target` 重新取，不受影响。

## 9. 验证

跑 005，开 validation layer + sync validation。

**该看到的：** 画面与改动前一致（clear 色 + 三角形），validation 无输出。

**跨帧行为**：`render_targets[frame % 2]` 两张轮转。帧 0 从 `eUndefined` 进；帧 2 时这张 image
被上一轮的出口 barrier 留在 `eTransferSrcOptimal`，而入口 barrier 的 `oldLayout` 仍是
`eUndefined`——合法，`eUndefined` 作为 `oldLayout` 表示"不关心旧内容"，而 `loadOp = eClear` 确实
不需要。所以循环不用追踪实际 layout。

**usage 位**：005 建 image 时给的是
`eColorAttachment | eTransferSrc`，而三个意图（`eDiscard` / `eColorAttachment` /
`eTransferSource`）要的并集正好是这两个，`begin()` 里那条 assert 应该过。故意把 `eTransferSrc`
去掉一次、确认 assert 会响，能验证这条路是活的。

**若出现 VUID 报 `oldLayout` 不匹配**：先看是不是 §7.2 的提前返回没改成 `if` 块（barrier 全空），
再看 `setExitUsage` 有没有漏。

**若画面黑但 validation 干净**：`resolved[color_index].in_pass_layout` 有没有写成
`resolved[color_index].entry_layout`——`eUndefined` 作为 `RenderingAttachmentInfo::imageLayout`
会让写入丢弃。

## 10. 本轮不做

- **静态路径接意图**（设计 §3.5.5、§3.6）：`StaticRenderInfo` 的七个 setter、
  `makeAttachmentReference` 转发工厂、`StaticRender::create()` 消费 entry/exit layout。005 的
  静态四行是注释掉的，接了也验证不到。
- **`register_unified_image_layouts`**（设计 §3.8）：`Swapchain.cpp` 四处硬编码 layout 是前置项
  （设计 §6），在那之前开 unified 会 UB。
- **`enableByRegionDependency()` 的实际使用**：接口按设计加上（第 3.3、6.2、7.4 步），但 005 不
  调。单 pass 无前序 producer，`eByRegion` 没有收益。

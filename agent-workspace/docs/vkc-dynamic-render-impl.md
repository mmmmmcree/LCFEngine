# vkc DynamicRender 实现文档

> 落地 dynamic rendering 路径,`imageLayout` 由 `DynamicRenderInfo` 自持 per-slot 数组。
> 目标:005 跑通。设计背景见 `vkc-attachment-set-authority-design.md`。

## 变更清单

| 文件 | 动作 |
|---|---|
| `include/vk_core/utils/format_utils.h` | 新增 |
| `include/vk_core/pipeline/graphics/info_structs.h` | 补 2 个 getter + 新增 `DynamicRenderInfo` |
| `src/pipeline/graphics/info_structs.cpp` | 新增 `DynamicRenderInfo` 构造函数 |
| `include/vk_core/pipeline/graphics/DynamicRender.h` | 新增 |
| `src/pipeline/graphics/DynamicRender.cpp` | 新增 |
| `include/vk_core/pipeline/graphics/DynamicGraphicsPipeline.h` | 类名补 `s` + 改 `create` 签名 + 补成员 |
| `src/pipeline/graphics/DynamicGraphicsPipeline.cpp` | 新增 |
| `examples/.../005_hello_static_pipeline_main.cpp` | 改 |

`libs/vk_core/CMakeLists.txt` 用 `GLOB_RECURSE`,新增 .cpp 不用改 CMake。

---

## 1. `utils/format_utils.h`(新增)

```cpp
#pragma once

#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_format_traits.hpp>
#include <string_view>

namespace lcf::vkc::utils {

//- depth component is named "D", stencil "S" (e.g. eD24UnormS8Uint -> "D", "S")
constexpr vk::ImageAspectFlags aspect_of(vk::Format format) noexcept
{
    if (format == vk::Format::eUndefined) { return vk::ImageAspectFlagBits::eNone; }
    vk::ImageAspectFlags aspect_flags;
    uint8_t component_count = vk::componentCount(format);
    for (uint8_t component = 0; component < component_count; ++component) {
        std::string_view component_name = vk::componentName(format, component);
        if (component_name == "D") { aspect_flags |= vk::ImageAspectFlagBits::eDepth; }
        else if (component_name == "S") { aspect_flags |= vk::ImageAspectFlagBits::eStencil; }
    }
    if (not aspect_flags) { aspect_flags = vk::ImageAspectFlagBits::eColor; }
    return aspect_flags;
}

} // namespace lcf::vkc::utils
```

005 用不到(无 depth stencil),但 `DynamicRender::create` 要 include 它来判定 ds 的 aspect。

## 2. `info_structs.h` 改动

### 2.1 `AttachmentDescriptionInfo` 补 2 个 getter

在 `getStoreOp()` 之后(`info_structs.h:746` 附近)插入:

```cpp
    const vk::AttachmentLoadOp & getStencilLoadOp() const noexcept { return m_description.root().stencilLoadOp; }
    const vk::AttachmentStoreOp & getStencilStoreOp() const noexcept { return m_description.root().stencilStoreOp; }
```

静态路径整体 `static_cast<vk::AttachmentDescription2>` 带走全部字段,不需要;动态路径要把
一份描述拆成 `pDepthAttachment` 与 `pStencilAttachment` 两个结构,必须逐字段读。

### 2.2 前置声明

`DynamicRenderInfo` 已在 `info_structs.h:810` 前置声明、`:826` 是 `AttachmentSetInfo` 的
friend,无需改动。文件末尾(`StaticRenderInfo` 之后)追加 `DynamicRenderInfo` 定义。

### 2.3 `DynamicRenderInfo`(新增,接在 `StaticRenderInfo` 之后)

```cpp
class DynamicRenderInfo
{
    friend class DynamicRender;
    using Self = DynamicRenderInfo;
    using Root = vk::RenderingInfo;
    using LayoutList = std::vector<vk::ImageLayout>;
    using DescriptionList = std::vector<AttachmentDescriptionInfo>;
    using ColorResolveList = std::vector<std::pair<vk::ResolveModeFlagBits, uint32_t>>;
public:
    ~DynamicRenderInfo() noexcept = default;
    explicit DynamicRenderInfo(AttachmentSetInfo & attachments) noexcept;
    DynamicRenderInfo(const Self &) = delete;
    DynamicRenderInfo(Self &&) noexcept = default;
    Self & operator=(const Self &) = delete;
    Self & operator=(Self &&) noexcept = default;
    operator const Root &() const noexcept { return m_rendering.root(); }
public:
    template <utils::struct_extends_c<Root> T>
    T & requestExtension() noexcept { return m_rendering.template request<T>(); }
    Self & addFlags(vk::RenderingFlags flags) noexcept { m_rendering.root().flags |= flags; return *this; }
    Self & setViewMask(uint32_t view_mask) noexcept { m_rendering.root().setViewMask(view_mask); return *this; }
    Self & setLoadStoreOp(details::attachment_key_c auto key, vk::AttachmentLoadOp load, vk::AttachmentStoreOp store) noexcept
    {
        m_attachments.mutableAt(key).setLoadStoreOp(load, store);
        return *this;
    }
    Self & setStencilLoadStoreOp(DepthStencilAttachmentKey key, vk::AttachmentLoadOp load, vk::AttachmentStoreOp store) noexcept
    {
        m_attachments.mutableAt(key).setStencilLoadStoreOp(load, store);
        return *this;
    }
    //- dynamic rendering has a single in-render layout per slot; there is no initial/final pair
    Self & setLayout(details::attachment_key_c auto key, vk::ImageLayout layout) noexcept
    {
        m_layouts[m_attachments.getIndex(key)] = layout;
        return *this;
    }
    uint32_t getColorAttachmentCount() const noexcept { return m_attachments.getColorAttachmentCount(); }
private:
    const DescriptionList & getAttachmentDescriptions() const noexcept { return m_attachments.m_descriptions; }
    const ColorResolveList & getColorResolveList() const noexcept { return m_attachments.m_color_resolve_list; }
    const LayoutList & getLayouts() const noexcept { return m_layouts; }
    bool hasDepthStencilAttachment() const noexcept { return m_attachments.m_has_depth_stencil; }
private:
    utils::DynamicStructureChain<Root> m_rendering;
    AttachmentSetInfo & m_attachments;
    LayoutList m_layouts; //- flat slot indexed, same order as m_attachments.m_descriptions
};
```

要点:

- **不提供 `setInitialFinalLayout`** —— 动态路径没有这个概念。
- **不提供 `addAttachmentFlags`** —— `vk::AttachmentDescriptionFlags` 只有 `eMayAlias`,
  在 `vk::RenderingAttachmentInfo` 里没有对应物,加了是死接口。
- 四个私有 getter 供 `DynamicRender`(friend)读。它们的存在是因为 `DynamicRender` 不是
  `AttachmentSetInfo` 的 friend,不该为它扩大 set 的 friend 列表。
- `setLayout` 调的 `m_attachments.getIndex(key)` 在 `AttachmentSetInfo` 里是私有的,
  `DynamicRenderInfo` 的 friend 身份(`info_structs.h:826`)使其可达。
- 与 `StaticRenderInfo` 的一处**有意不同**:`StaticRenderInfo::getAttachmentDescriptions()`
  是 public(`info_structs.h:996`),但 `StaticRender` 本来就是它的 friend,那个 public 是
  多余的。这里把四个 getter 全放 private,只靠 friend 暴露。其余部分(构造/拷贝移动/
  `operator const Root &`/`requestExtension`/成员排列)与 `StaticRenderInfo` 逐条对齐。

### 2.4 `info_structs.cpp` 追加构造函数

构造函数要按角色填默认布局,故不能内联在头里(需要读 set 的私有成员算段边界,写在 .cpp
更清楚,也与 `RenderTargetInfo::setSampleCount` 的风格一致):

```cpp
DynamicRenderInfo::DynamicRenderInfo(AttachmentSetInfo & attachments) noexcept :
    m_attachments(attachments),
    m_layouts(attachments.m_descriptions.size(), vk::ImageLayout::eColorAttachmentOptimal)
{
    if (m_attachments.m_has_depth_stencil) {
        m_layouts.back() = vk::ImageLayout::eDepthStencilAttachmentOptimal;
    }
}
```

color 段与 resolve 段的默认值都是 `eColorAttachmentOptimal`,只有末尾的 ds slot 例外,
所以一次 `resize` + 一次改末尾即可,不必分段循环。

## 3. `DynamicRender.h`(新增)

```cpp
#pragma once

#include <vulkan/vulkan.hpp>
#include <vector>
#include <optional>
#include <span>
#include <system_error>
#include "vk_core/utils/DynamicStructureChain.h"

namespace lcf::vkc {

class DynamicRenderInfo;
class RenderTarget;
class CommandBufferProxy;

class DynamicRenderScopeInfo
{
    using FormatList = std::span<const vk::Format>;
public:
    ~DynamicRenderScopeInfo() noexcept = default;
    DynamicRenderScopeInfo(
        FormatList color_formats,
        vk::Format depth_format,
        vk::Format stencil_format,
        uint32_t view_mask) noexcept :
        m_color_formats(color_formats),
        m_depth_format(depth_format),
        m_stencil_format(stencil_format),
        m_view_mask(view_mask) {}
    DynamicRenderScopeInfo(const DynamicRenderScopeInfo &) = default;
    DynamicRenderScopeInfo(DynamicRenderScopeInfo &&) noexcept = default;
    DynamicRenderScopeInfo & operator=(const DynamicRenderScopeInfo &) = default;
    DynamicRenderScopeInfo & operator=(DynamicRenderScopeInfo &&) noexcept = default;
public:
    const FormatList & getColorFormats() const noexcept { return m_color_formats; }
    const vk::Format & getDepthFormat() const noexcept { return m_depth_format; }
    const vk::Format & getStencilFormat() const noexcept { return m_stencil_format; }
    const uint32_t & getViewMask() const noexcept { return m_view_mask; }
private:
    //- views into the owning DynamicRender's vector; consumed synchronously by pipeline creation
    FormatList m_color_formats;
    vk::Format m_depth_format;
    vk::Format m_stencil_format;
    uint32_t m_view_mask;
};

class DynamicRender
{
    using Self = DynamicRender;
    using Root = vk::RenderingInfo;
    using AttachmentInfoList = std::vector<vk::RenderingAttachmentInfo>;
    using SlotList = std::vector<uint32_t>;
    using FormatList = std::vector<vk::Format>;
public:
    ~DynamicRender() noexcept = default;
    DynamicRender() noexcept = default;
    DynamicRender(const Self &) = delete;
    DynamicRender(Self &&) noexcept = default;
    Self & operator=(const Self &) = delete;
    Self & operator=(Self &&) noexcept = default;
public:
    //- no device object to build; everything is precomputed on the CPU. the error_code return
    //- keeps the shape symmetric with StaticRender::create and currently always yields {}
    std::error_code create(const DynamicRenderInfo & render_info) noexcept;
    void begin(CommandBufferProxy & cmd, const RenderTarget & render_target) noexcept;
    void end(CommandBufferProxy & cmd) noexcept;jj
    DynamicRenderScopeInfo makeScopeInfo() const noexcept;
private:
    //- owns a relinked copy of the info's chain, so create() does not outlive-depend on the info
    utils::DynamicStructureChain<Root> m_rendering;
    AttachmentInfoList m_color_attachments;
    //- color index -> resolve slot, AttachmentUnused when that color has no resolve
    SlotList m_resolve_slots;
    std::optional<vk::RenderingAttachmentInfo> m_depth_attachment;
    std::optional<vk::RenderingAttachmentInfo> m_stencil_attachment;
    uint32_t m_depth_stencil_slot = vk::AttachmentUnused;
    FormatList m_color_formats;
    vk::Format m_depth_format = vk::Format::eUndefined;
    vk::Format m_stencil_format = vk::Format::eUndefined;
};

} // namespace lcf::vkc
```

`m_color_attachments` 既是 create 时烘好的模板,也是 `begin` 就地改写 `imageView` /
`resolveImageView` / `clearValue` 的工作区 —— 每帧零分配。

## 4. `DynamicRender.cpp`(新增)

```cpp
#include "vk_core/pipeline/graphics/DynamicRender.h"
#include "vk_core/pipeline/graphics/RenderTarget.h"
#include "vk_core/pipeline/graphics/info_structs.h"
#include "vk_core/command/CommandBufferProxy.h"
#include "vk_core/utils/format_utils.h"

namespace lcf::vkc {

std::error_code DynamicRender::create(const DynamicRenderInfo & render_info) noexcept
{
    //- copy the whole chain, not just the root: begin() rebuilds vk::RenderingInfo every frame and
    //- its pNext must not point into render_info, which may die right after create(). the chain's
    //- copy constructor relinks pNext for us
    m_rendering = render_info.m_rendering;
    const auto & descriptions = render_info.getAttachmentDescriptions();
    const auto & color_resolve_list = render_info.getColorResolveList();
    const auto & layouts = render_info.getLayouts();
    uint32_t color_count = render_info.getColorAttachmentCount();
    m_color_attachments.clear();
    m_color_attachments.reserve(color_count);
    m_resolve_slots.clear();
    m_resolve_slots.reserve(color_count);
    m_color_formats.clear();
    m_color_formats.reserve(color_count);
    for (uint32_t color_index = 0; color_index < color_count; ++color_index) {
        const AttachmentDescriptionInfo & description = descriptions[color_index];
        auto [resolve_mode, resolve_slot] = color_resolve_list[color_index];
        //- imageView / resolveImageView / clearValue are filled per frame in begin()
        m_color_attachments.emplace_back()
            .setImageLayout(layouts[color_index])
            .setResolveMode(resolve_mode)
            .setLoadOp(description.getLoadOp())
            .setStoreOp(description.getStoreOp());
        if (resolve_slot != vk::AttachmentUnused) {
            m_color_attachments.back().setResolveImageLayout(layouts[resolve_slot]);
        }
        m_resolve_slots.emplace_back(resolve_slot);
        m_color_formats.emplace_back(description.getFormat());
    }
    m_depth_attachment.reset();
    m_stencil_attachment.reset();
    m_depth_stencil_slot = vk::AttachmentUnused;
    m_depth_format = vk::Format::eUndefined;
    m_stencil_format = vk::Format::eUndefined;
    if (render_info.hasDepthStencilAttachment()) {
        m_depth_stencil_slot = static_cast<uint32_t>(descriptions.size()) - 1u;
        const AttachmentDescriptionInfo & description = descriptions[m_depth_stencil_slot];
        //- one set slot, one description, but two vk structures with independent ops
        vk::ImageAspectFlags aspect_flags = utils::aspect_of(description.getFormat());
        if (aspect_flags & vk::ImageAspectFlagBits::eDepth) {
            m_depth_attachment.emplace()
                .setImageLayout(layouts[m_depth_stencil_slot])
                .setLoadOp(description.getLoadOp())
                .setStoreOp(description.getStoreOp());
            m_depth_format = description.getFormat();
        }
        if (aspect_flags & vk::ImageAspectFlagBits::eStencil) {
            m_stencil_attachment.emplace()
                .setImageLayout(layouts[m_depth_stencil_slot])
                .setLoadOp(description.getStencilLoadOp())
                .setStoreOp(description.getStencilStoreOp());
            m_stencil_format = description.getFormat();
        }
    }
    return {};
}

void DynamicRender::begin(CommandBufferProxy & cmd, const RenderTarget & render_target) noexcept
{
    auto image_views = render_target.viewAttachmentImageViews();
    const auto & clear_values = render_target.getClearValues();
    uint32_t color_count = static_cast<uint32_t>(m_color_attachments.size());
    for (uint32_t color_index = 0; color_index < color_count; ++color_index) {
        //- color slot == color ordinal, by the canonical [colors][resolves][ds?] layout
        m_color_attachments[color_index].setImageView(image_views[color_index])
            .setClearValue(clear_values[color_index]);
        uint32_t resolve_slot = m_resolve_slots[color_index];
        if (resolve_slot != vk::AttachmentUnused) {
            m_color_attachments[color_index].setResolveImageView(image_views[resolve_slot]);
        }
    }
    if (m_depth_attachment) {
        m_depth_attachment->setImageView(image_views[m_depth_stencil_slot])
            .setClearValue(clear_values[m_depth_stencil_slot]);
    }
    if (m_stencil_attachment) {
        m_stencil_attachment->setImageView(image_views[m_depth_stencil_slot])
            .setClearValue(clear_values[m_depth_stencil_slot]);
    }
    //- flags / viewMask / pNext come from the chain copy; the rest is per target
    vk::RenderingInfo rendering_info = m_rendering.root();
    rendering_info.setRenderArea(render_target.getRenderArea())
        .setLayerCount(render_target.getLayerCount())
        .setColorAttachments(m_color_attachments)
        .setPDepthAttachment(m_depth_attachment ? &m_depth_attachment.value() : nullptr)
        .setPStencilAttachment(m_stencil_attachment ? &m_stencil_attachment.value() : nullptr);
    cmd.beginRendering(rendering_info);
}

void DynamicRender::end(CommandBufferProxy & cmd) noexcept
{
    cmd.endRendering();
}

auto DynamicRender::makeScopeInfo() const noexcept -> DynamicRenderScopeInfo
{
    return { m_color_formats, m_depth_format, m_stencil_format, m_rendering.root().viewMask };
}

} // namespace lcf::vkc
```

`viewAttachmentImageViews()` 返回 `transform_view` over `std::vector`,是 random access,
`operator[]` 可用。`clear_values` 已按 slot 排好(`RenderTarget::build` 给每个 slot 都填了),
两者都不需要新增 `RenderTarget` 接口。

`setColorAttachments(m_color_attachments)` 同时设 `colorAttachmentCount` 与
`pColorAttachments`;指针指向成员 vector,`cmd.beginRendering` 同步消费,安全。

## 5. `DynamicGraphicsPipeline.h` 改动

现有类只有 `vk::UniquePipeline m_pipeline` 一个成员,且 `create`/`bind` **只有声明没有定义**
(`HEAD` 上就如此,不是这轮删出来的)。改成:

```cpp
#pragma once

#include <vulkan/vulkan.hpp>
#include <vector>

namespace lcf::vkc {

class GraphicsPipelineInfo;

class CommandBufferProxy;

class DynamicRenderScopeInfo;

class DynamicGraphicsPipeline
{
    using Self = DynamicGraphicsPipeline;
public:
    ~DynamicGraphicsPipeline() noexcept = default;
    DynamicGraphicsPipeline() noexcept = default;
    DynamicGraphicsPipeline(const Self &) noexcept = delete;
    DynamicGraphicsPipeline(Self &&) noexcept = default;
    Self & operator=(const Self &) noexcept = delete;
    Self & operator=(Self &&) noexcept = default;
    operator const vk::Pipeline &() const noexcept { return m_pipeline.get(); }
public:
    std::error_code create(
        vk::Device device,
        const GraphicsPipelineInfo & pipeline_info,
        const DynamicRenderScopeInfo & render_scope_info) noexcept;
    void bind(CommandBufferProxy & cmd) const noexcept;
    const vk::Pipeline & handle() const noexcept { return m_pipeline.get(); }
private:
    vk::UniquePipeline m_pipeline;
    vk::UniquePipelineLayout m_pipeline_layout;
    std::vector<vk::UniqueShaderModule> m_shader_modules;
};

} // namespace lcf::vkc
```

三处与原声明的差异:类名补上 `s`(原 `DynamicGraphicPipeline` 与文件名和
`StaticGraphicsPipeline` 都不一致,现在没有调用者,改的成本最低);`create` 加
`render_scope_info` 参数;`bind` 参数从 `vk::CommandBuffer` 改成 `CommandBufferProxy &`,
与 `StaticGraphicsPipeline::bind` 一致;补 layout 与 shader module 两个成员。

## 6. `DynamicGraphicsPipeline.cpp`(新增)

与 `StaticGraphicsPipeline.cpp` 的差异只有两处:去掉
`setRenderPass`/`setSubpass`,加 `setPNext`。其余逐字相同。

```cpp
#include "vk_core/pipeline/graphics/DynamicGraphicsPipeline.h"
#include "vk_core/pipeline/graphics/DynamicRender.h"
#include "vk_core/pipeline/graphics/info_structs.h"
#include "vk_core/command/CommandBufferProxy.h"
#include <ranges>

namespace stdr = std::ranges;
namespace stdv = std::views;

namespace lcf::vkc {

std::error_code DynamicGraphicsPipeline::create(
    vk::Device device,
    const GraphicsPipelineInfo & pipeline_info,
    const DynamicRenderScopeInfo & render_scope_info) noexcept
{
    const ShaderProgramInfo & shader_program_info = pipeline_info.getShaderProgramInfo();
    auto shader_stage_infos_view = shader_program_info.viewStageInfos();
    std::size_t shader_stage_count = shader_stage_infos_view.size();
    std::vector<vk::PipelineShaderStageCreateInfo> pipeline_shader_stage_infos;
    std::vector<vk::PushConstantRange> push_constant_ranges;
    m_shader_modules.reserve(shader_stage_count);
    pipeline_shader_stage_infos.reserve(shader_stage_count);
    for (const auto & stage_info : shader_stage_infos_view) {
        vk::ShaderModuleCreateInfo shader_module_create_info;
        shader_module_create_info.setCode(stage_info.getCode());
        try {
            m_shader_modules.emplace_back(device.createShaderModuleUnique(shader_module_create_info));
        } catch (const vk::SystemError & e) {
            return e.code();
        }
        vk::PipelineShaderStageCreateInfo pipeline_shader_stage_info;
        pipeline_shader_stage_info.setStage(stage_info.getStage())
            .setModule(m_shader_modules.back().get())
            .setPName(stage_info.getEntryPoint().c_str())
            .setPSpecializationInfo(&stage_info.getSpecializationInfo());
        pipeline_shader_stage_infos.emplace_back(pipeline_shader_stage_info);
        push_constant_ranges.append_range(stage_info.getPushConstantRanges());
    }
    auto descriptor_set_layouts = shader_program_info.viewDescriptorSetLayouts() | stdr::to<std::vector>();
    vk::PipelineLayoutCreateInfo pipeline_layout_create_info;
    pipeline_layout_create_info.setPushConstantRanges(push_constant_ranges)
        .setSetLayouts(descriptor_set_layouts);
    try {
        m_pipeline_layout = device.createPipelineLayoutUnique(pipeline_layout_create_info);
    } catch (const vk::SystemError & e) {
        return e.code();
    }
    //- replaces render pass + subpass; formats must match the vk::RenderingInfo used at draw time.
    //- note there is no samples field here: sample count comes only from pMultisampleState
    vk::PipelineRenderingCreateInfo rendering_create_info;
    rendering_create_info.setViewMask(render_scope_info.getViewMask())
        .setColorAttachmentFormats(render_scope_info.getColorFormats())
        .setDepthAttachmentFormat(render_scope_info.getDepthFormat())
        .setStencilAttachmentFormat(render_scope_info.getStencilFormat());
    vk::GraphicsPipelineCreateInfo pipeline_create_info;
    pipeline_create_info.setPNext(&rendering_create_info)
        .setFlags(pipeline_info.getFlags())
        .setStages(pipeline_shader_stage_infos)
        .setPVertexInputState(&static_cast<const vk::PipelineVertexInputStateCreateInfo &>(pipeline_info.getVertexInputInfo()))
        .setPInputAssemblyState(&static_cast<const vk::PipelineInputAssemblyStateCreateInfo &>(pipeline_info.getInputAssemblyStateInfo()))
        .setPTessellationState(&static_cast<const vk::PipelineTessellationStateCreateInfo &>(pipeline_info.getTessellationStateInfo()))
        .setPViewportState(&static_cast<const vk::PipelineViewportStateCreateInfo &>(pipeline_info.getViewportStateInfo()))
        .setPRasterizationState(&static_cast<const vk::PipelineRasterizationStateCreateInfo &>(pipeline_info.getRasterizationStateInfo()))
        .setPMultisampleState(&static_cast<const vk::PipelineMultisampleStateCreateInfo &>(pipeline_info.getMultisampleStateInfo()))
        .setPDepthStencilState(&static_cast<const vk::PipelineDepthStencilStateCreateInfo &>(pipeline_info.getDepthStencilStateInfo()))
        .setPColorBlendState(&static_cast<const vk::PipelineColorBlendStateCreateInfo &>(pipeline_info.getColorBlendStateInfo()))
        .setPDynamicState(&static_cast<const vk::PipelineDynamicStateCreateInfo &>(pipeline_info.getDynamicStateInfo()))
        .setLayout(m_pipeline_layout.get());
    try {
        auto [result, pipeline] = device.createGraphicsPipelineUnique(nullptr, pipeline_create_info);
        if (result != vk::Result::eSuccess) { return result; }
        m_pipeline = std::move(pipeline);
    } catch (const vk::SystemError & e) {
        return e.code();
    }
    return {};
}

void DynamicGraphicsPipeline::bind(CommandBufferProxy & cmd) const noexcept
{
    cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, this->handle());
}

} // namespace lcf::vkc
```

`setColorAttachmentFormats` 接受 `ArrayProxyNoTemporaries`,`std::span<const vk::Format>`
可直接传。`renderPass` 留 `nullptr`(默认值)即为 dynamic rendering 模式。

先照抄不抽象:两条 `create` 有约 60 行逐字重复,但 scope 概念还没定型,现在抽会把它焊死。

## 7. 005 示例改动

005 当前 L163-204 那段(attachment set / render target / image)**完全不用动** —— 这段是
路径无关的,这也正是 attachment set 抽象要达到的效果。改的只有下面四处。

### 7.1 头文件

```diff
-#include "vk_core/pipeline/graphics/StaticGraphicsPipeline.h"
-#include "vk_core/pipeline/graphics/StaticRender.h"
+#include "vk_core/pipeline/graphics/DynamicGraphicsPipeline.h"
+#include "vk_core/pipeline/graphics/DynamicRender.h"
 #include "vk_core/pipeline/graphics/RenderTarget.h"
```

### 7.2 开 feature(L70)

```diff
     device_ext_manifest.addRequiredFeature(vkc::utils::t_feature_bit<&vk::PhysicalDeviceVulkan13Features::synchronization2>)
+        .addRequiredFeature(vkc::utils::t_feature_bit<&vk::PhysicalDeviceVulkan13Features::dynamicRendering>)
         .addRequiredFeature(vkc::utils::t_feature_bit<&vk::PhysicalDeviceVulkan11Features::shaderDrawParameters>);
```

### 7.3 render + pipeline(替换 L207-235)

```cpp
    //- create dynamic render
    vkc::DynamicRenderInfo dynamic_render_info {attachment_set};
    dynamic_render_info.setLoadStoreOp(color_key, vk::AttachmentLoadOp::eClear, vk::AttachmentStoreOp::eStore);
    vkc::DynamicRender dynamic_render;
    if (auto ec = dynamic_render.create(dynamic_render_info)) {
        lcf_log_error("Failed to create dynamic_render: {}", ec.message());
        return 1;
    }
    //- create graphics pipeline with dynamic rendering

    vkc::ViewportStateInfo viewport_state_info;
    viewport_state_info.addViewport(0, 0, width, height)
        .addScissor(0, 0, width, height);
    vkc::ColorBlendStateInfo color_blend_state_info {dynamic_render_info.getColorAttachmentCount()};
    vkc::GraphicsPipelineInfo graphic_pipeline_info;
    vkc::DynamicGraphicsPipeline dynamic_graphics_pipeline;
    graphic_pipeline_info.setShaderProgramInfo(std::move(shader_program_info))
        .setViewportStateInfo(viewport_state_info)
        .setColorBlendStateInfo(color_blend_state_info);
    if (auto ec = dynamic_graphics_pipeline.create(device, graphic_pipeline_info, dynamic_render.makeScopeInfo())) {
        lcf_log_error("Failed to create dynamic_graphics_pipeline: {}", ec.message());
        return 1;
    }
```

比 static 路径少三样:`SubpassDescriptionInfo`(没有 subpass 概念)、
`setInitialFinalLayout`(没有这个概念,见 §2.3)、`create` 的 `device` 参数(不造对象)。
`dynamic_render_info` 必须活到 `create` 返回,之后可以死 —— `DynamicRender` 已经持有
relink 过的链拷贝。`vk::Device device = device_context.getDevice();`(原 L206)保留,
pipeline 还要用。

### 7.4 布局转换(渲染循环)

static 路径靠 render pass 的 `initialLayout`/`finalLayout` 隐式转换布局,dynamic 路径
没有这个机制,必须自己下 barrier。`Swapchain::present` 只 barrier swapchain 自己的图像,
要求源图像**进来时已经是** `eTransferSrcOptimal`(`Swapchain.cpp:118` 的
`setSrcImageLayout`),它不会替你转,也不会转回去。所以两条 barrier 都得例子自己发。

```diff
             vk::CommandBufferBeginInfo cmd_begin_info {};
             cmd.begin(cmd_begin_info);
-            static_render.begin(cmd, render_target);
-            static_graphics_pipeline.bind(cmd);
+            const vkc::Attachment & color_attachment = render_target.getAttachment(color_key);
+            vk::ImageSubresourceRange color_range = color_attachment.getDescription().getSubresourceRange();
+            //- oldLayout eUndefined discards the previous contents, which is what we want:
+            //- loadOp is eClear, and the only prior state is either the initial layout or
+            //- eTransferSrcOptimal left by last frame's blit. no need to track it.
+            vk::ImageMemoryBarrier2 to_color_barrier;
+            to_color_barrier.setImage(color_attachment.getImage())
+                .setOldLayout(vk::ImageLayout::eUndefined)
+                .setNewLayout(vk::ImageLayout::eColorAttachmentOptimal)
+                .setSubresourceRange(color_range)
+                .setSrcStageMask(vk::PipelineStageFlagBits2::eAllCommands)
+                .setSrcAccessMask(vk::AccessFlagBits2::eNone)
+                .setDstStageMask(vk::PipelineStageFlagBits2::eColorAttachmentOutput)
+                .setDstAccessMask(vk::AccessFlagBits2::eColorAttachmentWrite);
+            vk::DependencyInfo to_color_dep_info;
+            to_color_dep_info.setImageMemoryBarriers(to_color_barrier);
+            cmd.pipelineBarrier2(to_color_dep_info);
+            dynamic_render.begin(cmd, render_target);
+            dynamic_graphics_pipeline.bind(cmd);
             cmd.draw(3, 1, 0, 0);
-            static_render.end(cmd);
+            dynamic_render.end(cmd);
+            vk::ImageMemoryBarrier2 to_transfer_src_barrier;
+            to_transfer_src_barrier.setImage(color_attachment.getImage())
+                .setOldLayout(vk::ImageLayout::eColorAttachmentOptimal)
+                .setNewLayout(vk::ImageLayout::eTransferSrcOptimal)
+                .setSubresourceRange(color_range)
+                .setSrcStageMask(vk::PipelineStageFlagBits2::eColorAttachmentOutput)
+                .setSrcAccessMask(vk::AccessFlagBits2::eColorAttachmentWrite)
+                .setDstStageMask(vk::PipelineStageFlagBits2::eBlit)
+                .setDstAccessMask(vk::AccessFlagBits2::eTransferRead);
+            vk::DependencyInfo to_transfer_src_dep_info;
+            to_transfer_src_dep_info.setImageMemoryBarriers(to_transfer_src_barrier);
+            cmd.pipelineBarrier2(to_transfer_src_dep_info);
             cmd.end();
```

`present_image` 那行(原 L275)顺手改成复用上面的引用:

```diff
-            const vkc::Image & present_image = render_target.getAttachment(color_key).getImage();
+            const vkc::Image & present_image = color_attachment.getImage();
```

`CommandBufferProxy` 公开继承 `vk::CommandBuffer`,`pipelineBarrier2`/`beginRendering`
直接可调,不用额外包装。队列族索引不设 —— 默认 `vk::QueueFamilyIgnored`,同队列无所有权
转移,和 `Swapchain.cpp` 里的写法一致。

这两条 barrier **不进库**。stage/access 的选择依赖前后帧怎么用这张图,是 RenderGraph 层
的判断;`vk_core/sync` 现在只有 timeline semaphore,不该为了这个例子加 barrier helper。

## 8. 落地顺序

1. `format_utils.h` —— 独立,无依赖,可单独编过。
2. `info_structs.h/.cpp` 的两个 getter + `DynamicRenderInfo` —— 编过即可,此时还没有使用者。
3. `DynamicRender.h/.cpp` —— 编过。
4. `DynamicGraphicsPipeline.h/.cpp` —— 编过。
5. 005 的四处改动 —— 跑起来。开着 validation layer 跑,重点看有没有
   `VUID-vkCmdBeginRendering-*` 和 layout 相关的报错。
6. 确认 005 画面和 static 路径一致(白底三角形)后,再考虑把 `DynamicRender` 接进
   RenderGraph 层。

`libs/vk_core/CMakeLists.txt` 是 `file(GLOB_RECURSE SOURCES "src/*.cpp")`,新增两个 .cpp
不用改 CMake,但要重跑一次 configure 让 glob 生效。

## 9. 本文档范围外的已知问题

这些是这轮读代码时看到的,和 dynamic render 路径无关,不在本文档改动范围内:

- `SubpassDescriptionInfo::m_flat_depth_stencil_ref`(`info_structs.h:707`)在 move/copy
  后悬空 —— 只影响 static 路径。
- resolve 段的槽位顺序是 `unordered_map` 的哈希序,结果正确但不确定,跨运行可能不同。
- `DynamicStructureChain` 的拷贝构造只 relink `pNext`,不深拷内部数组指针。本文档的
  `DynamicRenderInfo` 没有带数组的扩展节点,所以现在是安全的;以后接
  `vk::RenderingFragmentDensityMapAttachmentInfoEXT` 之类要重新审。
- attachment key 的构造函数是 public,可以伪造一个 set 内越界的 index。
- `RenderTarget` 还缺 `setResolveAttachment`/`setDepthStencilAttachment`,所以本文档的
  多 attachment 代码路径在 005 里跑不到,只能靠单 color 覆盖。

之前提到的三个死类(`AttachmentStateInfo`/`AttachmentFormatRef`/`AttachmentStateRef`)、
`info_structs.cpp` 的 `stdr`/`stdv` 别名、`RenderTarget.h` 的多余 include 都已经清掉了,
不用再处理。

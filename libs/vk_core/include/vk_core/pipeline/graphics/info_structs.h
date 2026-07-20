#pragma once

#include <vulkan/vulkan.hpp>
#include <array>
#include <vector>
#include <span>
#include <optional>
#include "vk_core/utils/DynamicStructureChain.h"
#include "vk_core/pipeline/shader/info_structs.h"

namespace lcf::vkc {

class VertexInputBindingInfo
{
    using Self = VertexInputBindingInfo;
    using Root = vk::VertexInputBindingDescription2EXT;
public:
    VertexInputBindingInfo(uint32_t binding = 0u, uint32_t stride = 0u,
        vk::VertexInputRate input_rate = vk::VertexInputRate::eVertex, uint32_t divisor = 1u) noexcept
    {
        m_binding.root().setBinding(binding).setStride(stride).setInputRate(input_rate).setDivisor(divisor);
    }
    operator const Root &() const noexcept { return m_binding.root(); }
    operator vk::VertexInputBindingDescription() const noexcept
    {
        const auto & r = m_binding.root();
        return { r.binding, r.stride, r.inputRate };
    }
public:
    template <utils::struct_extends_c<Root> T>
    T & requestExtension() noexcept { return m_binding.template request<T>(); }
    Self & setDivisor(uint32_t divisor) noexcept { m_binding.root().setDivisor(divisor); return *this; }
    const uint32_t & getBinding() const noexcept { return m_binding.root().binding; }
    const uint32_t & getDivisor() const noexcept { return m_binding.root().divisor; }
private:
    utils::DynamicStructureChain<Root> m_binding;
};

class VertexInputAttributeInfo
{
    using Self = VertexInputAttributeInfo;
    using Root = vk::VertexInputAttributeDescription2EXT;
public:
    VertexInputAttributeInfo(uint32_t location = 0u, uint32_t binding = 0u,
        vk::Format format = vk::Format::eUndefined, uint32_t offset = 0u) noexcept
    {
        m_attribute.root().setLocation(location).setBinding(binding).setFormat(format).setOffset(offset);
    }
    operator const Root &() const noexcept { return m_attribute.root(); }
    operator vk::VertexInputAttributeDescription() const noexcept
    {
        const auto & r = m_attribute.root();
        return { r.location, r.binding, r.format, r.offset };
    }
public:
    template <utils::struct_extends_c<Root> T>
    T & requestExtension() noexcept { return m_attribute.template request<T>(); }
    const uint32_t & getLocation() const noexcept { return m_attribute.root().location; }
    const uint32_t & getBinding() const noexcept { return m_attribute.root().binding; }
    const vk::Format & getFormat() const noexcept { return m_attribute.root().format; }
    const uint32_t & getOffset() const noexcept { return m_attribute.root().offset; }
private:
    utils::DynamicStructureChain<Root> m_attribute;
};

class VertexInputInfo
{
    using Self = VertexInputInfo;
    using Root = vk::PipelineVertexInputStateCreateInfo;
    using BindingInfoList = std::vector<VertexInputBindingInfo>;
    using AttributeInfoList = std::vector<VertexInputAttributeInfo>;
    using BindingList = std::vector<vk::VertexInputBindingDescription>;
    using AttributeList = std::vector<vk::VertexInputAttributeDescription>;
public:
    ~VertexInputInfo() noexcept = default;
    VertexInputInfo() noexcept = default;
    VertexInputInfo(const Self & other) = default;
    VertexInputInfo(Self && other) noexcept = default;
    Self & operator=(const Self & other) = default;
    Self & operator=(Self && other) noexcept = default;
    operator const Root &() const noexcept { return m_vertex_input_state.root(); }
public:
    template <utils::struct_extends_c<Root> T>
    T & requestExtension() noexcept { return m_vertex_input_state.template request<T>(); }
    Self & addBinding(const VertexInputBindingInfo & binding)
    {
        m_binding_infos.emplace_back(binding);
        m_flat_bindings.emplace_back(m_binding_infos.back());
        m_vertex_input_state.root().setVertexBindingDescriptions(m_flat_bindings);
        return *this;
    }
    Self & addAttribute(const VertexInputAttributeInfo & attribute)
    {
        m_attribute_infos.emplace_back(attribute);
        m_flat_attributes.emplace_back(m_attribute_infos.back());
        m_vertex_input_state.root().setVertexAttributeDescriptions(m_flat_attributes);
        return *this;
    }
    const BindingInfoList & getBindings() const noexcept { return m_binding_infos; }
    const AttributeInfoList & getAttributes() const noexcept { return m_attribute_infos; }
private:
    utils::DynamicStructureChain<Root> m_vertex_input_state;
    BindingInfoList m_binding_infos;
    BindingList m_flat_bindings;
    AttributeInfoList m_attribute_infos;
    AttributeList m_flat_attributes;
};

class InputAssemblyStateInfo
{
    using Self = InputAssemblyStateInfo;
    using Root = vk::PipelineInputAssemblyStateCreateInfo;
public:
    ~InputAssemblyStateInfo() noexcept = default;
    InputAssemblyStateInfo(
        vk::PipelineInputAssemblyStateCreateFlags flags = {},
        vk::PrimitiveTopology topology = vk::PrimitiveTopology::eTriangleList,
        vk::Bool32 primitive_restart_enable = false) noexcept
    {
        m_input_assembly_state.root()
            .setFlags(flags)
            .setTopology(topology)
            .setPrimitiveRestartEnable(primitive_restart_enable);
    }
    InputAssemblyStateInfo(const Self & other) noexcept = default;
    InputAssemblyStateInfo(Self && other) noexcept = default;
    Self & operator=(const Self & other) noexcept = default;
    Self & operator=(Self && other) noexcept = default;
    operator const Root &() const noexcept { return m_input_assembly_state.root(); }
public:
    template <utils::struct_extends_c<Root> T>
    T & requestExtension() noexcept { return m_input_assembly_state.template request<T>(); }
    Self & addFlags(vk::PipelineInputAssemblyStateCreateFlags flags) noexcept { m_input_assembly_state.root().setFlags(m_input_assembly_state.root().flags | flags); return *this; }
    Self & setTopology(vk::PrimitiveTopology topology) noexcept { m_input_assembly_state.root().setTopology(topology); return *this; }
    Self & enablePrimitiveRestart() noexcept { m_input_assembly_state.root().setPrimitiveRestartEnable(true); return *this; }
    const vk::PipelineInputAssemblyStateCreateFlags & getFlags() const noexcept { return m_input_assembly_state.root().flags; }
    const vk::PrimitiveTopology & getTopology() const noexcept { return m_input_assembly_state.root().topology; }
    const vk::Bool32 & isPrimitiveRestartEnabled() const noexcept { return m_input_assembly_state.root().primitiveRestartEnable; }
private:
    utils::DynamicStructureChain<Root> m_input_assembly_state;
};

class TessellationStateInfo
{
    using Self = TessellationStateInfo;
    using Root = vk::PipelineTessellationStateCreateInfo;
public:
    ~TessellationStateInfo() noexcept = default;
    TessellationStateInfo() noexcept = default;
    TessellationStateInfo(const Self & other) noexcept = default;
    TessellationStateInfo(Self && other) noexcept = default;
    Self & operator=(const Self & other) noexcept = default;
    Self & operator=(Self && other) noexcept = default;
    operator const Root &() const noexcept { return m_tessellation_state.root(); }
public:
    template <utils::struct_extends_c<Root> T>
    T & requestExtension() noexcept { return m_tessellation_state.template request<T>(); }
    Self & addFlags(vk::PipelineTessellationStateCreateFlags flags) noexcept { m_tessellation_state.root().setFlags(m_tessellation_state.root().flags | flags); return *this; }
    Self & setPatchControlPoints(uint32_t count) noexcept { m_tessellation_state.root().setPatchControlPoints(count); return *this; }
    const vk::PipelineTessellationStateCreateFlags & getFlags() const noexcept { return m_tessellation_state.root().flags; }
    const uint32_t & getPatchControlPoints() const noexcept { return m_tessellation_state.root().patchControlPoints; }
private:
    utils::DynamicStructureChain<Root> m_tessellation_state;
};

class RasterizationStateInfo
{
    using Self = RasterizationStateInfo;
    using Root = vk::PipelineRasterizationStateCreateInfo;
public:
    ~RasterizationStateInfo() noexcept = default;
    RasterizationStateInfo() { m_rasterization_state.root().setLineWidth(1.0f); }
    RasterizationStateInfo(const Self & other) = default;
    RasterizationStateInfo(Self && other) noexcept = default;
    Self & operator=(const Self & other) = default;
    Self & operator=(Self && other) noexcept = default;
    operator const Root &() const noexcept { return m_rasterization_state.root(); }
public:
    template <utils::struct_extends_c<Root> T>
    T & requestExtension() noexcept { return m_rasterization_state.template request<T>(); }
    Self & addFlags(vk::PipelineRasterizationStateCreateFlags flags) noexcept { m_rasterization_state.root().setFlags(m_rasterization_state.root().flags | flags); return *this; }
    Self & enableDepthClamp() noexcept { m_rasterization_state.root().setDepthClampEnable(true); return *this; }
    Self & enableRasterizerDiscard() noexcept { m_rasterization_state.root().setRasterizerDiscardEnable(true); return *this; }
    Self & setPolygonMode(vk::PolygonMode mode) noexcept { m_rasterization_state.root().setPolygonMode(mode); return *this; }
    Self & setCullMode(vk::CullModeFlags mode) noexcept { m_rasterization_state.root().setCullMode(mode); return *this; }
    Self & setFrontFace(vk::FrontFace front_face) noexcept { m_rasterization_state.root().setFrontFace(front_face); return *this; }
    Self & enableDepthBias(float constant_factor = 0.0f, float clamp = 0.0f, float slope_factor = 0.0f) noexcept
    {
        m_rasterization_state.root()
            .setDepthBiasEnable(true)
            .setDepthBiasConstantFactor(constant_factor)
            .setDepthBiasClamp(clamp)
            .setDepthBiasSlopeFactor(slope_factor);
        return *this;
    }
    Self & setLineWidth(float width) noexcept { m_rasterization_state.root().setLineWidth(width); return *this; }
    const vk::PipelineRasterizationStateCreateFlags & getFlags() const noexcept { return m_rasterization_state.root().flags; }
    const vk::Bool32 & isDepthClampEnabled() const noexcept { return m_rasterization_state.root().depthClampEnable; }
    const vk::Bool32 & isRasterizerDiscardEnabled() const noexcept { return m_rasterization_state.root().rasterizerDiscardEnable; }
    const vk::PolygonMode & getPolygonMode() const noexcept { return m_rasterization_state.root().polygonMode; }
    const vk::CullModeFlags & getCullMode() const noexcept { return m_rasterization_state.root().cullMode; }
    const vk::FrontFace & getFrontFace() const noexcept { return m_rasterization_state.root().frontFace; }
    const vk::Bool32 & isDepthBiasEnabled() const noexcept { return m_rasterization_state.root().depthBiasEnable; }
    const float & getLineWidth() const noexcept { return m_rasterization_state.root().lineWidth; }
private:
    utils::DynamicStructureChain<Root> m_rasterization_state;
};

class MultisampleStateInfo
{
    using Self = MultisampleStateInfo;
    using Root = vk::PipelineMultisampleStateCreateInfo;
    using SampleMaskList = std::vector<vk::SampleMask>;
public:
    ~MultisampleStateInfo() noexcept = default;
    MultisampleStateInfo(
        vk::PipelineMultisampleStateCreateFlags flags = {},
        vk::SampleCountFlagBits rasterization_samples = vk::SampleCountFlagBits::e1,
        vk::Bool32 sample_shading_enable = false,
        float min_sample_shading = 1.0f,
        SampleMaskList sample_mask = {},
        vk::Bool32 alpha_to_coverage_enable = false,
        vk::Bool32 alpha_to_one_enable = false) :
        m_sample_mask(std::move(sample_mask))
    {
        m_multisample_state.root()
            .setFlags(flags)
            .setRasterizationSamples(rasterization_samples)
            .setSampleShadingEnable(sample_shading_enable)
            .setMinSampleShading(min_sample_shading)
            .setPSampleMask(m_sample_mask.empty() ? nullptr : m_sample_mask.data())
            .setAlphaToCoverageEnable(alpha_to_coverage_enable)
            .setAlphaToOneEnable(alpha_to_one_enable);
    }
    MultisampleStateInfo(const Self & other) = default;
    MultisampleStateInfo(Self && other) noexcept = default;
    Self & operator=(const Self & other) = default;
    Self & operator=(Self && other) noexcept = default;
    operator const Root &() const noexcept { return m_multisample_state.root(); }
public:
    template <utils::struct_extends_c<Root> T>
    T & requestExtension() noexcept { return m_multisample_state.template request<T>(); }
    Self & addFlags(vk::PipelineMultisampleStateCreateFlags flags) noexcept { m_multisample_state.root().setFlags(m_multisample_state.root().flags | flags); return *this; }
    Self & setRasterizationSamples(vk::SampleCountFlagBits samples) noexcept { m_multisample_state.root().setRasterizationSamples(samples); return *this; }
    Self & enableSampleShading(float min_sample_shading = 1.0f) noexcept
    {
        m_multisample_state.root()
            .setSampleShadingEnable(true)
            .setMinSampleShading(min_sample_shading);
        return *this;
    }
    Self & setSampleMask(SampleMaskList sample_mask) noexcept
    {
        m_sample_mask = std::move(sample_mask);
        m_multisample_state.root().setPSampleMask(m_sample_mask.empty() ? nullptr : m_sample_mask.data());
        return *this;
    }
    Self & enableAlphaToCoverage() noexcept { m_multisample_state.root().setAlphaToCoverageEnable(true); return *this; }
    Self & enableAlphaToOne() noexcept { m_multisample_state.root().setAlphaToOneEnable(true); return *this; }
    const vk::PipelineMultisampleStateCreateFlags & getFlags() const noexcept { return m_multisample_state.root().flags; }
    const vk::SampleCountFlagBits & getRasterizationSamples() const noexcept { return m_multisample_state.root().rasterizationSamples; }
    const vk::Bool32 & isSampleShadingEnabled() const noexcept { return m_multisample_state.root().sampleShadingEnable; }
    const float & getMinSampleShading() const noexcept { return m_multisample_state.root().minSampleShading; }
    const vk::Bool32 & isAlphaToCoverageEnabled() const noexcept { return m_multisample_state.root().alphaToCoverageEnable; }
    const vk::Bool32 & isAlphaToOneEnabled() const noexcept { return m_multisample_state.root().alphaToOneEnable; }
private:
    utils::DynamicStructureChain<Root> m_multisample_state;
    SampleMaskList m_sample_mask;
};

class DepthStencilStateInfo
{
    using Self = DepthStencilStateInfo;
    using Root = vk::PipelineDepthStencilStateCreateInfo;
public:
    ~DepthStencilStateInfo() noexcept = default;
    DepthStencilStateInfo(
        vk::PipelineDepthStencilStateCreateFlags flags = {},
        vk::Bool32 depth_test_enable = false,
        vk::Bool32 depth_write_enable = false,
        vk::CompareOp depth_compare_op = vk::CompareOp::eNever,
        vk::Bool32 depth_bounds_test_enable = false,
        vk::Bool32 stencil_test_enable = false,
        const vk::StencilOpState & front = {},
        const vk::StencilOpState & back = {},
        float min_depth_bounds = 0.0f,
        float max_depth_bounds = 0.0f) noexcept
    {
        m_depth_stencil_state.root()
            .setFlags(flags)
            .setDepthTestEnable(depth_test_enable)
            .setDepthWriteEnable(depth_write_enable)
            .setDepthCompareOp(depth_compare_op)
            .setDepthBoundsTestEnable(depth_bounds_test_enable)
            .setStencilTestEnable(stencil_test_enable)
            .setFront(front)
            .setBack(back)
            .setMinDepthBounds(min_depth_bounds)
            .setMaxDepthBounds(max_depth_bounds);
    }
    DepthStencilStateInfo(const Self & other) noexcept = default;
    DepthStencilStateInfo(Self && other) noexcept = default;
    Self & operator=(const Self & other) noexcept = default;
    Self & operator=(Self && other) noexcept = default;
    operator const Root &() const noexcept { return m_depth_stencil_state.root(); }
public:
    template <utils::struct_extends_c<Root> T>
    T & requestExtension() noexcept { return m_depth_stencil_state.template request<T>(); }
    Self & addFlags(vk::PipelineDepthStencilStateCreateFlags flags) noexcept { m_depth_stencil_state.root().setFlags(m_depth_stencil_state.root().flags | flags); return *this; }
    Self & enableDepthTest(bool write_enable = true, vk::CompareOp compare_op = vk::CompareOp::eLess) noexcept
    {
        m_depth_stencil_state.root()
            .setDepthTestEnable(true)
            .setDepthWriteEnable(write_enable)
            .setDepthCompareOp(compare_op);
        return *this;
    }
    Self & enableStencilTest(const vk::StencilOpState & front = {}, const vk::StencilOpState & back = {}) noexcept
    {
        m_depth_stencil_state.root()
            .setStencilTestEnable(true)
            .setFront(front)
            .setBack(back);
        return *this;
    }
    Self & enableDepthBoundsTest(float min_bounds = 0.0f, float max_bounds = 1.0f) noexcept
    {
        m_depth_stencil_state.root()
            .setDepthBoundsTestEnable(true)
            .setMinDepthBounds(min_bounds)
            .setMaxDepthBounds(max_bounds);
        return *this;
    }
    const vk::PipelineDepthStencilStateCreateFlags & getFlags() const noexcept { return m_depth_stencil_state.root().flags; }
    const vk::Bool32 & isDepthTestEnabled() const noexcept { return m_depth_stencil_state.root().depthTestEnable; }
    const vk::Bool32 & isDepthWriteEnabled() const noexcept { return m_depth_stencil_state.root().depthWriteEnable; }
    const vk::CompareOp & getDepthCompareOp() const noexcept { return m_depth_stencil_state.root().depthCompareOp; }
    const vk::Bool32 & isDepthBoundsTestEnabled() const noexcept { return m_depth_stencil_state.root().depthBoundsTestEnable; }
    const vk::Bool32 & isStencilTestEnabled() const noexcept { return m_depth_stencil_state.root().stencilTestEnable; }
    const vk::StencilOpState & getFrontStencilState() const noexcept { return m_depth_stencil_state.root().front; }
    const vk::StencilOpState & getBackStencilState() const noexcept { return m_depth_stencil_state.root().back; }
    const float & getMinDepthBounds() const noexcept { return m_depth_stencil_state.root().minDepthBounds; }
    const float & getMaxDepthBounds() const noexcept { return m_depth_stencil_state.root().maxDepthBounds; }
private:
    utils::DynamicStructureChain<Root> m_depth_stencil_state;
};

class ColorBlendStateInfo
{
    using Self = ColorBlendStateInfo;
    using Root = vk::PipelineColorBlendStateCreateInfo;
    using ColorBlendAttachmentStateList = std::vector<vk::PipelineColorBlendAttachmentState>;
    using BlendConstants = std::array<float, 4>;
public:
    ~ColorBlendStateInfo() noexcept = default;
    ColorBlendStateInfo(
        uint32_t blend_state_count,
        vk::PipelineColorBlendStateCreateFlags flags = {},
        vk::Bool32 logic_op_enable = false,
        vk::LogicOp logic_op = vk::LogicOp::eCopy,
        const BlendConstants & blend_constants = {})
    {
        m_color_blend_attachment_states.resize(blend_state_count,
            vk::PipelineColorBlendAttachmentState{}.setColorWriteMask(vk::FlagTraits<vk::ColorComponentFlagBits>::allFlags));
        m_color_blend_state.root()
            .setFlags(flags)
            .setLogicOpEnable(logic_op_enable)
            .setLogicOp(logic_op)
            .setBlendConstants(blend_constants)
            .setAttachments(m_color_blend_attachment_states);
    }
    ColorBlendStateInfo(const Self & other) = default;
    ColorBlendStateInfo(Self && other) noexcept = default;
    Self & operator=(const Self & other) = default;
    Self & operator=(Self && other) noexcept = default;
    operator const Root &() const noexcept { return m_color_blend_state.root(); }
public:
    template <utils::struct_extends_c<Root> T>
    T & requestExtension() noexcept { return m_color_blend_state.template request<T>(); }
    Self & addFlags(vk::PipelineColorBlendStateCreateFlags flags) noexcept { m_color_blend_state.root().setFlags(m_color_blend_state.root().flags | flags); return *this; }
    Self & enableLogicOp(vk::LogicOp logic_op = vk::LogicOp::eCopy) noexcept
    {
        m_color_blend_state.root()
            .setLogicOpEnable(true)
            .setLogicOp(logic_op);
        return *this;
    }
    Self & setBlendConstants(const BlendConstants & constants) noexcept { m_color_blend_state.root().setBlendConstants(constants); return *this; }
    Self & setColorBlendAttachmentState(
        uint32_t  attachment_index,
        vk::BlendFactor src_color, vk::BlendFactor dst_color, vk::BlendOp color_op,
        vk::BlendFactor src_alpha, vk::BlendFactor dst_alpha, vk::BlendOp alpha_op,
        vk::ColorComponentFlags write_mask = vk::FlagTraits<vk::ColorComponentFlagBits>::allFlags)
    {
        m_color_blend_attachment_states[attachment_index] = vk::PipelineColorBlendAttachmentState {true, src_color, dst_color, color_op, src_alpha, dst_alpha, alpha_op, write_mask};
        m_color_blend_state.root().setAttachments(m_color_blend_attachment_states);
        return *this;
    }
    const vk::PipelineColorBlendStateCreateFlags & getFlags() const noexcept { return m_color_blend_state.root().flags; }
    const vk::Bool32 & isLogicOpEnabled() const noexcept { return m_color_blend_state.root().logicOpEnable; }
    const vk::LogicOp & getLogicOp() const noexcept { return m_color_blend_state.root().logicOp; }
    const ColorBlendAttachmentStateList & getColorBlendAttachmentStates() const noexcept { return m_color_blend_attachment_states; }
private:
    utils::DynamicStructureChain<Root> m_color_blend_state;
    ColorBlendAttachmentStateList m_color_blend_attachment_states;
};

class ViewportStateInfo
{
    using Self = ViewportStateInfo;
    using Root = vk::PipelineViewportStateCreateInfo;
    using ViewportList = std::vector<vk::Viewport>;
    using ScissorList = std::vector<vk::Rect2D>;
public:
    ~ViewportStateInfo() noexcept = default;
    ViewportStateInfo(
        vk::PipelineViewportStateCreateFlags flags = {},
        ViewportList viewports = {},
        ScissorList scissors = {}) :
        m_viewports(std::move(viewports)),
        m_scissors(std::move(scissors))
    {
        m_viewport_state.root().setFlags(flags).setViewports(m_viewports).setScissors(m_scissors);
    }
    ViewportStateInfo(const Self & other) = default;
    ViewportStateInfo(Self && other) noexcept = default;
    Self & operator=(const Self & other) = default;
    Self & operator=(Self && other) noexcept = default;
    operator const Root &() const noexcept { return m_viewport_state.root(); }
public:
    template <utils::struct_extends_c<Root> T>
    T & requestExtension() noexcept { return m_viewport_state.template request<T>(); }
    Self & addFlags(vk::PipelineViewportStateCreateFlags flags) noexcept { m_viewport_state.root().setFlags(m_viewport_state.root().flags | flags); return *this; }
    Self & addViewport(float x, float y, float width, float height, float min_depth = 0.0f, float max_depth = 1.0f)
    {
        m_viewports.emplace_back(x, y, width, height, min_depth, max_depth);
        m_viewport_state.root().setViewports(m_viewports);
        return *this;
    }
    Self & addScissor(int32_t offset_x, int32_t offset_y, uint32_t width, uint32_t height)
    {
        m_scissors.emplace_back(vk::Offset2D{offset_x, offset_y}, vk::Extent2D{width, height});
        m_viewport_state.root().setScissors(m_scissors);
        return *this;
    }
    const vk::PipelineViewportStateCreateFlags & getFlags() const noexcept { return m_viewport_state.root().flags; }
    std::span<const vk::Viewport> getViewports() const noexcept { return m_viewports; }
    std::span<const vk::Rect2D> getScissors() const noexcept { return m_scissors; }
private:
    utils::DynamicStructureChain<Root> m_viewport_state;
    ViewportList m_viewports;
    ScissorList m_scissors;
};

class DynamicStateInfo
{
    using Self = DynamicStateInfo;
    using Root = vk::PipelineDynamicStateCreateInfo;
    using StateList = std::vector<vk::DynamicState>;
public:
    ~DynamicStateInfo() noexcept = default;
    DynamicStateInfo(
        vk::PipelineDynamicStateCreateFlags flags = {},
        StateList states = {}) :
        m_states(std::move(states))
    {
        m_dynamic_state.root().setFlags(flags).setDynamicStates(m_states);
    }
    DynamicStateInfo(const Self & other) = default;
    DynamicStateInfo(Self && other) noexcept = default;
    Self & operator=(const Self & other) = default;
    Self & operator=(Self && other) noexcept = default;
    operator const Root &() const noexcept { return m_dynamic_state.root(); }
public:
    template <utils::struct_extends_c<Root> T>
    T & requestExtension() noexcept { return m_dynamic_state.template request<T>(); }
    Self & addFlags(vk::PipelineDynamicStateCreateFlags flags) noexcept { m_dynamic_state.root().setFlags(m_dynamic_state.root().flags | flags); return *this; }
    Self & addDynamicState(vk::DynamicState state) { m_states.emplace_back(state); m_dynamic_state.root().setDynamicStates(m_states); return *this; }
    Self & setDynamicStates(StateList states) { m_states = std::move(states); m_dynamic_state.root().setDynamicStates(m_states); return *this; }
    const vk::PipelineDynamicStateCreateFlags & getFlags() const noexcept { return m_dynamic_state.root().flags; }
    std::span<const vk::DynamicState> getDynamicStates() const noexcept { return m_states; }
private:
    utils::DynamicStructureChain<Root> m_dynamic_state;
    StateList m_states;
};

class GraphicsPipelineInfo
{
    using Self = GraphicsPipelineInfo;
public:
    ~GraphicsPipelineInfo() noexcept = default;
    GraphicsPipelineInfo() = default;
    GraphicsPipelineInfo(const Self & other) = default;
    GraphicsPipelineInfo(Self && other) noexcept = default;
    Self & operator=(const Self & other) = default;
    Self & operator=(Self && other) noexcept = default;
public:
    Self & addFlags(vk::PipelineCreateFlagBits flags) noexcept { m_flags |= flags; return *this; }
    Self & setShaderProgramInfo(ShaderProgramInfo info) { m_shader_program_info = std::move(info); return *this; }
    Self & setVertexInputInfo(VertexInputInfo info) { m_vertex_input_info = std::move(info); return *this; }
    Self & setInputAssemblyStateInfo(const InputAssemblyStateInfo & info) { m_input_assembly_info = info; return *this; }
    Self & setTessellationStateInfo(const TessellationStateInfo & info) { m_tessellation_info = info; return *this; }
    Self & setRasterizationStateInfo(const RasterizationStateInfo & info) { m_rasterization_info = info; return *this; }
    Self & setMultisampleStateInfo(MultisampleStateInfo info) { m_multisample_info = std::move(info); return *this; }
    Self & setDepthStencilStateInfo(const DepthStencilStateInfo & info) { m_depth_stencil_info = info; return *this; }
    Self & setColorBlendStateInfo(ColorBlendStateInfo info) { m_color_blend_info = std::move(info); return *this; }
    Self & setViewportStateInfo(ViewportStateInfo info) { m_viewport_info = std::move(info); return *this; }
    Self & setDynamicStateInfo(DynamicStateInfo info) { m_dynamic_state_info = std::move(info); return *this; }

    const vk::PipelineCreateFlags & getFlags() const noexcept { return m_flags; }
    const ShaderProgramInfo & getShaderProgramInfo() const noexcept { return m_shader_program_info; }
    const VertexInputInfo & getVertexInputInfo() const noexcept { return m_vertex_input_info; }
    const InputAssemblyStateInfo & getInputAssemblyStateInfo() const noexcept { return m_input_assembly_info; }
    const TessellationStateInfo & getTessellationStateInfo() const noexcept { return m_tessellation_info; }
    const RasterizationStateInfo & getRasterizationStateInfo() const noexcept { return m_rasterization_info; }
    const MultisampleStateInfo & getMultisampleStateInfo() const noexcept { return m_multisample_info; }
    const DepthStencilStateInfo & getDepthStencilStateInfo() const noexcept { return m_depth_stencil_info; }
    const ColorBlendStateInfo & getColorBlendStateInfo() const noexcept { return m_color_blend_info; }
    const ViewportStateInfo & getViewportStateInfo() const noexcept { return m_viewport_info; }
    const DynamicStateInfo & getDynamicStateInfo() const noexcept { return m_dynamic_state_info; }
private:
    vk::PipelineCreateFlags m_flags = {};
    ShaderProgramInfo m_shader_program_info;
    VertexInputInfo m_vertex_input_info;
    InputAssemblyStateInfo m_input_assembly_info;
    TessellationStateInfo m_tessellation_info;
    RasterizationStateInfo m_rasterization_info;
    MultisampleStateInfo m_multisample_info;
    DepthStencilStateInfo m_depth_stencil_info;
    ColorBlendStateInfo m_color_blend_info {0};
    ViewportStateInfo m_viewport_info;
    DynamicStateInfo m_dynamic_state_info;
};

class AttachmentStateInfo
{
    using Self = AttachmentStateInfo;
public:
    ~AttachmentStateInfo() noexcept = default;
    constexpr AttachmentStateInfo(
        vk::AttachmentDescriptionFlags flags = {},
        vk::AttachmentLoadOp load_op = vk::AttachmentLoadOp::eDontCare,
        vk::AttachmentStoreOp store_op = vk::AttachmentStoreOp::eDontCare,
        vk::AttachmentLoadOp stencil_load_op = vk::AttachmentLoadOp::eDontCare,
        vk::AttachmentStoreOp stencil_store_op = vk::AttachmentStoreOp::eDontCare,
        vk::ImageLayout initial_layout = vk::ImageLayout::eUndefined,
        vk::ImageLayout final_layout = vk::ImageLayout::eUndefined) noexcept :
        m_flags(flags),
        m_load_op(load_op),
        m_store_op(store_op),
        m_stencil_load_op(stencil_load_op),
        m_stencil_store_op(stencil_store_op),
        m_initial_layout(initial_layout),
        m_final_layout(final_layout) {}
    AttachmentStateInfo(const Self & other) noexcept = default;
    AttachmentStateInfo(Self && other) noexcept = default;
    Self & operator=(const Self & other) noexcept = default;
    Self & operator=(Self && other) noexcept = default;
public:
    Self & addFlags(vk::AttachmentDescriptionFlags flags) noexcept { m_flags |= flags; return *this; }
    Self & setLoadStoreOp(vk::AttachmentLoadOp load_op, vk::AttachmentStoreOp store_op) noexcept
    {
        m_load_op = load_op;
        m_store_op = store_op;
        return *this;
    }
    Self & setStencilLoadStoreOp(vk::AttachmentLoadOp load_op, vk::AttachmentStoreOp store_op) noexcept
    {
        m_stencil_load_op = load_op;
        m_stencil_store_op = store_op;
        return *this;
    }
    Self & setLayouts(vk::ImageLayout initial_layout, vk::ImageLayout final_layout) noexcept
    {
        m_initial_layout = initial_layout;
        m_final_layout = final_layout;
        return *this;
    }
    const vk::AttachmentDescriptionFlags & getFlags() const noexcept { return m_flags; }
    const vk::AttachmentLoadOp & getLoadOp() const noexcept { return m_load_op; }
    const vk::AttachmentStoreOp & getStoreOp() const noexcept { return m_store_op; }
    const vk::AttachmentLoadOp & getStencilLoadOp() const noexcept { return m_stencil_load_op; }
    const vk::AttachmentStoreOp & getStencilStoreOp() const noexcept { return m_stencil_store_op; }
    const vk::ImageLayout & getInitialLayout() const noexcept { return m_initial_layout; }
    const vk::ImageLayout & getFinalLayout() const noexcept { return m_final_layout; }
private:
    vk::AttachmentDescriptionFlags m_flags;
    vk::AttachmentLoadOp m_load_op;
    vk::AttachmentStoreOp m_store_op;
    vk::AttachmentLoadOp m_stencil_load_op;
    vk::AttachmentStoreOp m_stencil_store_op;
    vk::ImageLayout m_initial_layout;
    vk::ImageLayout m_final_layout;
};

class AttachmentReferenceInfo
{
    using Self = AttachmentReferenceInfo;
    using Root = vk::AttachmentReference2;
public:
    ~AttachmentReferenceInfo() noexcept = default;
    AttachmentReferenceInfo(
        uint32_t attachment = vk::AttachmentUnused,
        vk::ImageLayout layout = vk::ImageLayout::eUndefined,
        vk::ImageAspectFlags aspect_mask = {}) noexcept
    {
        this->setAttachment(attachment)
            .setLayout(layout)
            .setAspectMask(aspect_mask);
    }
    AttachmentReferenceInfo(const Self & other) noexcept = default;
    AttachmentReferenceInfo(Self && other) noexcept = default;
    Self & operator=(const Self & other) noexcept = default;
    Self & operator=(Self && other) noexcept = default;
    operator vk::AttachmentReference() const noexcept { return { this->getAttachment(), this->getLayout() }; }
    operator const Root &() const noexcept { return m_attachment_reference.root(); }
public:
    template <utils::struct_extends_c<Root> T>
    T & requestExtension() noexcept { return m_attachment_reference.template request<T>(); }
    Self & setAttachment(uint32_t attachment) noexcept { m_attachment_reference.root().setAttachment(attachment); return *this; }
    Self & setLayout(vk::ImageLayout layout) noexcept { m_attachment_reference.root().setLayout(layout); return *this; }
    Self & setAspectMask(vk::ImageAspectFlags aspect_mask) noexcept { m_attachment_reference.root().setAspectMask(aspect_mask); return *this; }
    const uint32_t & getAttachment() const noexcept { return m_attachment_reference.root().attachment; }
    const vk::ImageLayout & getLayout() const noexcept { return m_attachment_reference.root().layout; }
private:
    utils::DynamicStructureChain<Root> m_attachment_reference;
};

class SubpassDependencyInfo
{
    using Self = SubpassDependencyInfo;
    using Root = vk::SubpassDependency2;
public:
    ~SubpassDependencyInfo() noexcept = default;
    SubpassDependencyInfo(
        uint32_t src_subpass = vk::SubpassExternal,
        uint32_t dst_subpass = 0u,
        vk::PipelineStageFlags src_stage_mask = {},
        vk::PipelineStageFlags dst_stage_mask = {},
        vk::AccessFlags src_access_mask = {},
        vk::AccessFlags dst_access_mask = {},
        vk::DependencyFlags dependency_flags = {},
        int32_t view_offset = 0) noexcept
    {
        m_subpass_dependency.root()
            .setSrcSubpass(src_subpass)
            .setDstSubpass(dst_subpass)
            .setSrcStageMask(src_stage_mask)
            .setDstStageMask(dst_stage_mask)
            .setSrcAccessMask(src_access_mask)
            .setDstAccessMask(dst_access_mask)
            .setDependencyFlags(dependency_flags)
            .setViewOffset(view_offset);
    }
    SubpassDependencyInfo(const Self & other) noexcept = default;
    SubpassDependencyInfo(Self && other) noexcept = default;
    Self & operator=(const Self & other) noexcept = default;
    Self & operator=(Self && other) noexcept = default;
    operator vk::SubpassDependency() const noexcept
    {
        const auto & root = m_subpass_dependency.root();
        return vk::SubpassDependency {
            root.srcSubpass, root.dstSubpass, root.srcStageMask, root.dstStageMask,
            root.srcAccessMask, root.dstAccessMask, root.dependencyFlags,
        };
    }
    operator const Root &() const noexcept { return m_subpass_dependency.root(); }
public:
    template <utils::struct_extends_c<Root> T>
    T & requestExtension() noexcept { return m_subpass_dependency.template request<T>(); }
    Self & setSubpasses(uint32_t src_subpass, uint32_t dst_subpass) noexcept
    {
        m_subpass_dependency.root().setSrcSubpass(src_subpass).setDstSubpass(dst_subpass);
        return *this;
    }
    Self & setStageMasks(vk::PipelineStageFlags src, vk::PipelineStageFlags dst) noexcept
    {
        m_subpass_dependency.root().setSrcStageMask(src).setDstStageMask(dst);
        return *this;
    }
    Self & setAccessMasks(vk::AccessFlags src, vk::AccessFlags dst) noexcept
    {
        m_subpass_dependency.root().setSrcAccessMask(src).setDstAccessMask(dst);
        return *this;
    }
    Self & addDependencyFlags(vk::DependencyFlags flags) noexcept { m_subpass_dependency.root().setDependencyFlags(m_subpass_dependency.root().dependencyFlags | flags); return *this; }
    Self & setViewOffset(int32_t view_offset) noexcept { m_subpass_dependency.root().setViewOffset(view_offset); return *this; }
private:
    utils::DynamicStructureChain<Root> m_subpass_dependency;
};

class SubpassDescriptionInfo
{
    using Self = SubpassDescriptionInfo;
    using Root = vk::SubpassDescription2;
    using ReferenceInfoList = std::vector<AttachmentReferenceInfo>;
    using PreserveList = std::vector<uint32_t>;
    using ReferenceList2 = std::vector<vk::AttachmentReference2>;
public:
    ~SubpassDescriptionInfo() noexcept = default;
    SubpassDescriptionInfo(
        vk::SubpassDescriptionFlags flags = {},
        vk::PipelineBindPoint bind_point = vk::PipelineBindPoint::eGraphics,
        uint32_t view_mask = 0u) noexcept
    {
        m_subpass.root().setFlags(flags).setPipelineBindPoint(bind_point).setViewMask(view_mask);
    }
    SubpassDescriptionInfo(const Self & other) = default;
    SubpassDescriptionInfo(Self && other) noexcept = default;
    Self & operator=(const Self & other) = default;
    Self & operator=(Self && other) noexcept = default;
    operator const Root &() const noexcept { return m_subpass.root(); }
public:
    template <utils::struct_extends_c<Root> T>
    T & requestExtension() noexcept { return m_subpass.template request<T>(); }
    Self & addFlags(vk::SubpassDescriptionFlags flags) noexcept { m_subpass.root().setFlags(m_subpass.root().flags | flags); return *this; }
    Self & setBindPoint(vk::PipelineBindPoint bind_point) noexcept { m_subpass.root().setPipelineBindPoint(bind_point); return *this; }
    Self & setViewMask(uint32_t view_mask) noexcept { m_subpass.root().setViewMask(view_mask); return *this; }
    Self & addColorAttachment(const AttachmentReferenceInfo & reference)
    {
        m_color_references.emplace_back(reference);
        m_flat_color_refs.emplace_back(static_cast<const vk::AttachmentReference2 &>(m_color_references.back()));
        m_subpass.root().setColorAttachments(m_flat_color_refs);
        return *this;
    }
    Self & addInputAttachment(const AttachmentReferenceInfo & reference)
    {
        m_input_references.emplace_back(reference);
        m_flat_input_refs.emplace_back(static_cast<const vk::AttachmentReference2 &>(m_input_references.back()));
        m_subpass.root().setInputAttachments(m_flat_input_refs);
        return *this;
    }
    Self & addResolveAttachment(const AttachmentReferenceInfo & reference)
    {
        m_resolve_references.emplace_back(reference);
        m_flat_resolve_refs.emplace_back(static_cast<const vk::AttachmentReference2 &>(m_resolve_references.back()));
        m_subpass.root().setPResolveAttachments(m_flat_resolve_refs.data());
        return *this;
    }
    Self & setDepthStencilAttachment(const AttachmentReferenceInfo & reference)
    {
        m_depth_stencil_reference = reference;
        m_flat_depth_stencil_ref = static_cast<const vk::AttachmentReference2 &>(*m_depth_stencil_reference);
        m_subpass.root().setPDepthStencilAttachment(&m_flat_depth_stencil_ref);
        return *this;
    }
    Self & addPreserveAttachment(uint32_t attachment)
    {
        m_preserve_attachments.emplace_back(attachment);
        m_subpass.root().setPreserveAttachments(m_preserve_attachments);
        return *this;
    }
    uint32_t getColorAttachmentReferenceCount() const noexcept { return static_cast<uint32_t>(m_color_references.size()); }
    const ReferenceInfoList & getColorAttachmentReferences() const noexcept { return m_color_references; }
    const ReferenceInfoList & getResolveAttachmentReferences() const noexcept { return m_resolve_references; }
    bool hasDepthStencil() const noexcept { return m_depth_stencil_reference.has_value(); }
    const AttachmentReferenceInfo & getDepthStencilAttachmentReference() const noexcept { return *m_depth_stencil_reference; }
private:
    utils::DynamicStructureChain<Root> m_subpass;
    ReferenceInfoList m_input_references;
    ReferenceInfoList m_color_references;
    ReferenceInfoList m_resolve_references;
    std::optional<AttachmentReferenceInfo> m_depth_stencil_reference;
    PreserveList m_preserve_attachments;
    ReferenceList2 m_flat_input_refs;
    ReferenceList2 m_flat_color_refs;
    ReferenceList2 m_flat_resolve_refs;
    vk::AttachmentReference2 m_flat_depth_stencil_ref;
};

class RenderTargetInfo
{
    using Self = RenderTargetInfo;
    using FormatList = std::vector<vk::Format>;
    using ResolveModeList = std::vector<vk::ResolveModeFlagBits>;
public:
    ~RenderTargetInfo() noexcept = default;
    RenderTargetInfo(
        vk::Extent2D extent = {},
        vk::Format depth_stencil_format = vk::Format::eUndefined,
        vk::SampleCountFlagBits sample_count = vk::SampleCountFlagBits::e1) noexcept :
        m_extent(extent),
        m_depth_stencil_format(depth_stencil_format),
        m_sample_count(sample_count) {}
    RenderTargetInfo(const Self & other) noexcept = default;
    RenderTargetInfo(Self && other) noexcept = default;
    Self & operator=(const Self & other) noexcept = default;
    Self & operator=(Self && other) noexcept = default;
public:
    Self & setExtent(const vk::Extent2D & extent) noexcept { m_extent = extent; return *this; }
    Self & addColorAttachment(vk::Format format, vk::ResolveModeFlagBits resolve_mode = vk::ResolveModeFlagBits::eNone) noexcept
    {
        m_color_formats.emplace_back(format);
        m_color_resolve_modes.emplace_back(resolve_mode);
        return *this;
    }
    Self & setSampleCount(vk::SampleCountFlagBits sample_count) noexcept { m_sample_count = sample_count; return *this; }
    Self & setDepthStencilFormat(vk::Format depth_stencil_format) noexcept { m_depth_stencil_format = depth_stencil_format; return *this; }
    const vk::Extent2D & getExtent() const noexcept { return m_extent; }
    const FormatList & getColorFormats() const noexcept { return m_color_formats; }
    const vk::Format & getColorFormat(uint32_t index) const noexcept { return m_color_formats[index]; }
    const vk::SampleCountFlagBits & getSampleCount() const noexcept { return m_sample_count; }
    bool hasDepthStencilFormat() const noexcept { return m_depth_stencil_format != vk::Format::eUndefined; }
    const vk::Format & getDepthStencilFormat() const noexcept { return m_depth_stencil_format; }
    vk::ResolveModeFlagBits getColorResolveMode(uint32_t color_index) const noexcept { return m_color_resolve_modes[color_index]; }
    bool isColorResolved(uint32_t color_index) const noexcept { return m_color_resolve_modes[color_index] != vk::ResolveModeFlagBits::eNone; }
    bool hasAnyResolve() const noexcept
    {
        return std::to_underlying(m_sample_count) > 1 and
            std::ranges::any_of(m_color_resolve_modes, [](auto m) { return m != vk::ResolveModeFlagBits::eNone; });
    }
private:
    vk::Extent2D m_extent;
    FormatList m_color_formats;
    ResolveModeList m_color_resolve_modes;
    vk::Format m_depth_stencil_format;
    vk::SampleCountFlagBits m_sample_count;
};

class RenderingInfo
{
    using Self = RenderingInfo;
    using AttachmentStateList = std::vector<AttachmentStateInfo>;
    using SubpassInfoList = std::vector<SubpassDescriptionInfo>;
    using DependencyInfoList = std::vector<SubpassDependencyInfo>;
    using ViewMaskList = std::vector<uint32_t>;
public:
    ~RenderingInfo() noexcept = default;
    RenderingInfo() = default;
    RenderingInfo(const Self & other) = default;
    RenderingInfo(Self && other) noexcept = default;
    Self & operator=(const Self & other) = default;
    Self & operator=(Self && other) noexcept = default;
public:
    Self & addFlags(vk::RenderPassCreateFlags flags) noexcept { m_flags |= flags; return *this; }
    Self & addAttachmentState(AttachmentStateInfo state) { m_attachment_states.emplace_back(std::move(state)); return *this; }
    Self & addSubpass(SubpassDescriptionInfo subpass) { m_subpasses.emplace_back(std::move(subpass)); return *this; }
    Self & addDependency(SubpassDependencyInfo dependency) { m_dependencies.emplace_back(std::move(dependency)); return *this; }
    Self & addCorrelatedViewMask(uint32_t view_mask) { m_correlated_view_masks.emplace_back(view_mask); return *this; }
    const vk::RenderPassCreateFlags & getFlags() const noexcept { return m_flags; }
    std::span<const AttachmentStateInfo> getAttachmentStates() const noexcept { return m_attachment_states; }
    std::span<const SubpassDescriptionInfo> getSubpasses() const noexcept { return m_subpasses; }
    std::span<const SubpassDependencyInfo> getDependencies() const noexcept { return m_dependencies; }
    const ViewMaskList & getCorrelatedViewMasks() const noexcept { return m_correlated_view_masks; }
private:
    vk::RenderPassCreateFlags m_flags;
    AttachmentStateList m_attachment_states;
    SubpassInfoList m_subpasses;
    DependencyInfoList m_dependencies;
    ViewMaskList m_correlated_view_masks;
};

} // namespace lcf::vkc

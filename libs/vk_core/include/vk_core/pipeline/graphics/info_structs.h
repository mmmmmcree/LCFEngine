#pragma once

#include <vulkan/vulkan.hpp>
#include <array>
#include <vector>
#include <span>
#include <optional>
#include "vk_core/pipeline/shader/info_structs.h"

namespace lcf::vkc {

class VertexInputInfo
{
    using Self = VertexInputInfo;
    using BindingList = std::vector<vk::VertexInputBindingDescription>;
    using AttributeList = std::vector<vk::VertexInputAttributeDescription>;
    using DivisorList = std::vector<uint32_t>;
    using BindingList2EXT = std::vector<vk::VertexInputBindingDescription2EXT>;
    using AttributeList2EXT = std::vector<vk::VertexInputAttributeDescription2EXT>;
public:
    ~VertexInputInfo() noexcept = default;
    VertexInputInfo() noexcept = default;
    VertexInputInfo(const Self & other) = default;
    VertexInputInfo(Self && other) noexcept = default;
    Self & operator=(const Self & other) = default;
    Self & operator=(Self && other) noexcept = default;
    operator vk::PipelineVertexInputStateCreateInfo() const noexcept
    {
        return vk::PipelineVertexInputStateCreateInfo{}.setVertexBindingDescriptions(m_bindings).setVertexAttributeDescriptions(m_attributes);
    }
public:
    Self & addBinding(uint32_t binding, uint32_t stride, vk::VertexInputRate input_rate = vk::VertexInputRate::eVertex, uint32_t divisor = 0u)
    {
        m_bindings.emplace_back(binding, stride, input_rate);
        m_binding_divisors.emplace_back(divisor);
        return *this;
    }
    Self & addAttribute(uint32_t location, uint32_t binding, vk::Format format, uint32_t offset)
    {
        m_attributes.emplace_back(location, binding, format, offset);
        return *this;
    }
    BindingList2EXT genBindingDescriptions2EXT() const;
    AttributeList2EXT genAttributeDescriptions2EXT() const;
    std::span<const vk::VertexInputBindingDescription> getBindings() const noexcept { return m_bindings; }
    std::span<const vk::VertexInputAttributeDescription> getAttributes() const noexcept { return m_attributes; }
private:
    BindingList m_bindings;
    DivisorList m_binding_divisors;
    AttributeList m_attributes;
};

class InputAssemblyStateInfo
{
    using Self = InputAssemblyStateInfo;
public:
    ~InputAssemblyStateInfo() noexcept = default;
    constexpr InputAssemblyStateInfo(
        vk::PipelineInputAssemblyStateCreateFlags flags = {},
        vk::PrimitiveTopology topology = vk::PrimitiveTopology::eTriangleList,
        vk::Bool32 primitive_restart_enable = false) noexcept :
        m_flags(flags),
        m_topology(topology),
        m_primitive_restart_enable(primitive_restart_enable) {}
    InputAssemblyStateInfo(const Self & other) noexcept = default;
    InputAssemblyStateInfo(Self && other) noexcept = default;
    Self & operator=(const Self & other) noexcept = default;
    Self & operator=(Self && other) noexcept = default;
    operator vk::PipelineInputAssemblyStateCreateInfo() const noexcept
    {
        return vk::PipelineInputAssemblyStateCreateInfo { m_flags, m_topology, m_primitive_restart_enable };
    }
public:
    Self & addFlags(vk::PipelineInputAssemblyStateCreateFlags flags) noexcept { m_flags |= flags; return *this; }
    Self & setTopology(vk::PrimitiveTopology topology) noexcept { m_topology = topology; return *this; }
    Self & enablePrimitiveRestart() noexcept { m_primitive_restart_enable = true; return *this; }
    const vk::PipelineInputAssemblyStateCreateFlags & getFlags() const noexcept { return m_flags; }
    const vk::PrimitiveTopology & getTopology() const noexcept { return m_topology; }
    const vk::Bool32 & isPrimitiveRestartEnabled() const noexcept { return m_primitive_restart_enable; }
private:
    vk::PipelineInputAssemblyStateCreateFlags m_flags;
    vk::PrimitiveTopology m_topology;
    vk::Bool32 m_primitive_restart_enable;
};

class TessellationStateInfo
{
    using Self = TessellationStateInfo;
public:
    ~TessellationStateInfo() noexcept = default;
    constexpr TessellationStateInfo(
        vk::PipelineTessellationStateCreateFlags flags = {},
        uint32_t patch_control_points = 0u) noexcept :
        m_flags(flags),
        m_patch_control_points(patch_control_points) {}
    TessellationStateInfo(const Self & other) noexcept = default;
    TessellationStateInfo(Self && other) noexcept = default;
    Self & operator=(const Self & other) noexcept = default;
    Self & operator=(Self && other) noexcept = default;
    operator vk::PipelineTessellationStateCreateInfo() const noexcept
    {
        return vk::PipelineTessellationStateCreateInfo { m_flags, m_patch_control_points };
    }
public:
    Self & addFlags(vk::PipelineTessellationStateCreateFlags flags) noexcept { m_flags |= flags; return *this; }
    Self & setPatchControlPoints(uint32_t count) noexcept { m_patch_control_points = count; return *this; }
    const vk::PipelineTessellationStateCreateFlags & getFlags() const noexcept { return m_flags; }
    const uint32_t & getPatchControlPoints() const noexcept { return m_patch_control_points; }
private:
    vk::PipelineTessellationStateCreateFlags m_flags;
    uint32_t m_patch_control_points;
};

class RasterizationStateInfo
{
    using Self = RasterizationStateInfo;
public:
    ~RasterizationStateInfo() noexcept = default;
    constexpr RasterizationStateInfo(
        vk::PipelineRasterizationStateCreateFlags flags = {},
        vk::Bool32 depth_clamp_enable = false,
        vk::Bool32 rasterizer_discard_enable = false,
        vk::PolygonMode polygon_mode = vk::PolygonMode::eFill,
        vk::CullModeFlags cull_mode = vk::CullModeFlagBits::eNone,
        vk::FrontFace front_face = vk::FrontFace::eCounterClockwise,
        vk::Bool32 depth_bias_enable = false,
        float depth_bias_constant_factor = 0.0f,
        float depth_bias_clamp = 0.0f,
        float depth_bias_slope_factor = 0.0f,
        float line_width = 1.0f) noexcept :
        m_flags(flags),
        m_depth_clamp_enable(depth_clamp_enable),
        m_rasterizer_discard_enable(rasterizer_discard_enable),
        m_polygon_mode(polygon_mode),
        m_cull_mode(cull_mode),
        m_front_face(front_face),
        m_depth_bias_enable(depth_bias_enable),
        m_depth_bias_constant_factor(depth_bias_constant_factor),
        m_depth_bias_clamp(depth_bias_clamp),
        m_depth_bias_slope_factor(depth_bias_slope_factor),
        m_line_width(line_width) {}
    RasterizationStateInfo(const Self & other) noexcept = default;
    RasterizationStateInfo(Self && other) noexcept = default;
    Self & operator=(const Self & other) noexcept = default;
    Self & operator=(Self && other) noexcept = default;
    operator vk::PipelineRasterizationStateCreateInfo() const noexcept
    {
        return vk::PipelineRasterizationStateCreateInfo {
            m_flags, m_depth_clamp_enable, m_rasterizer_discard_enable, m_polygon_mode, m_cull_mode, m_front_face,
            m_depth_bias_enable, m_depth_bias_constant_factor, m_depth_bias_clamp, m_depth_bias_slope_factor, m_line_width,
        };
    }
public:
    Self & addFlags(vk::PipelineRasterizationStateCreateFlags flags) noexcept { m_flags |= flags; return *this; }
    Self & enableDepthClamp() noexcept { m_depth_clamp_enable = true; return *this; }
    Self & enableRasterizerDiscard() noexcept { m_rasterizer_discard_enable = true; return *this; }
    Self & setPolygonMode(vk::PolygonMode mode) noexcept { m_polygon_mode = mode; return *this; }
    Self & setCullMode(vk::CullModeFlags mode) noexcept { m_cull_mode = mode; return *this; }
    Self & setFrontFace(vk::FrontFace front_face) noexcept { m_front_face = front_face; return *this; }
    Self & enableDepthBias(float constant_factor = 0.0f, float clamp = 0.0f, float slope_factor = 0.0f) noexcept
    {
        m_depth_bias_enable = true;
        m_depth_bias_constant_factor = constant_factor;
        m_depth_bias_clamp = clamp;
        m_depth_bias_slope_factor = slope_factor;
        return *this;
    }
    Self & setLineWidth(float width) noexcept { m_line_width = width; return *this; }
    const vk::PipelineRasterizationStateCreateFlags & getFlags() const noexcept { return m_flags; }
    const vk::Bool32 & isDepthClampEnabled() const noexcept { return m_depth_clamp_enable; }
    const vk::Bool32 & isRasterizerDiscardEnabled() const noexcept { return m_rasterizer_discard_enable; }
    const vk::PolygonMode & getPolygonMode() const noexcept { return m_polygon_mode; }
    const vk::CullModeFlags & getCullMode() const noexcept { return m_cull_mode; }
    const vk::FrontFace & getFrontFace() const noexcept { return m_front_face; }
    const vk::Bool32 & isDepthBiasEnabled() const noexcept { return m_depth_bias_enable; }
    const float & getLineWidth() const noexcept { return m_line_width; }
private:
    vk::PipelineRasterizationStateCreateFlags m_flags;
    vk::Bool32 m_depth_clamp_enable;
    vk::Bool32 m_rasterizer_discard_enable;
    vk::PolygonMode m_polygon_mode;
    vk::CullModeFlags m_cull_mode;
    vk::FrontFace m_front_face;
    vk::Bool32 m_depth_bias_enable;
    float m_depth_bias_constant_factor;
    float m_depth_bias_clamp;
    float m_depth_bias_slope_factor;
    float m_line_width;
};

class MultisampleStateInfo
{
    using Self = MultisampleStateInfo;
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
        vk::Bool32 alpha_to_one_enable = false) noexcept :
        m_flags(flags),
        m_rasterization_samples(rasterization_samples),
        m_sample_shading_enable(sample_shading_enable),
        m_min_sample_shading(min_sample_shading),
        m_sample_mask(std::move(sample_mask)),
        m_alpha_to_coverage_enable(alpha_to_coverage_enable),
        m_alpha_to_one_enable(alpha_to_one_enable) {}
    MultisampleStateInfo(const Self & other) = default;
    MultisampleStateInfo(Self && other) noexcept = default;
    Self & operator=(const Self & other) = default;
    Self & operator=(Self && other) noexcept = default;
    operator vk::PipelineMultisampleStateCreateInfo() const noexcept
    {
        return vk::PipelineMultisampleStateCreateInfo {
            m_flags,
            m_rasterization_samples,
            m_sample_shading_enable, m_min_sample_shading,
            m_sample_mask.data(),
            m_alpha_to_coverage_enable, m_alpha_to_one_enable,
        };
    }
public:
    Self & addFlags(vk::PipelineMultisampleStateCreateFlags flags) noexcept { m_flags |= flags; return *this; }
    Self & setRasterizationSamples(vk::SampleCountFlagBits samples) noexcept { m_rasterization_samples = samples; return *this; }
    Self & enableSampleShading(float min_sample_shading = 1.0f) noexcept
    {
        m_sample_shading_enable = true;
        m_min_sample_shading = min_sample_shading;
        return *this;
    }
    Self & setSampleMask(SampleMaskList sample_mask) noexcept { m_sample_mask = std::move(sample_mask); return *this; }
    Self & enableAlphaToCoverage() noexcept { m_alpha_to_coverage_enable = true; return *this; }
    Self & enableAlphaToOne() noexcept { m_alpha_to_one_enable = true; return *this; }
    const vk::PipelineMultisampleStateCreateFlags & getFlags() const noexcept { return m_flags; }
    const vk::SampleCountFlagBits & getRasterizationSamples() const noexcept { return m_rasterization_samples; }
    const vk::Bool32 & isSampleShadingEnabled() const noexcept { return m_sample_shading_enable; }
    const float & getMinSampleShading() const noexcept { return m_min_sample_shading; }
    const vk::Bool32 & isAlphaToCoverageEnabled() const noexcept { return m_alpha_to_coverage_enable; }
    const vk::Bool32 & isAlphaToOneEnabled() const noexcept { return m_alpha_to_one_enable; }
private:
    vk::PipelineMultisampleStateCreateFlags m_flags;
    vk::SampleCountFlagBits m_rasterization_samples;
    vk::Bool32 m_sample_shading_enable;
    float m_min_sample_shading;
    SampleMaskList m_sample_mask;
    vk::Bool32 m_alpha_to_coverage_enable;
    vk::Bool32 m_alpha_to_one_enable;
};

class DepthStencilStateInfo
{
    using Self = DepthStencilStateInfo;
public:
    ~DepthStencilStateInfo() noexcept = default;
    constexpr DepthStencilStateInfo(
        vk::PipelineDepthStencilStateCreateFlags flags = {},
        vk::Bool32 depth_test_enable = false,
        vk::Bool32 depth_write_enable = false,
        vk::CompareOp depth_compare_op = vk::CompareOp::eNever,
        vk::Bool32 depth_bounds_test_enable = false,
        vk::Bool32 stencil_test_enable = false,
        const vk::StencilOpState & front = {},
        const vk::StencilOpState & back = {},
        float min_depth_bounds = 0.0f,
        float max_depth_bounds = 0.0f) noexcept :
        m_flags(flags),
        m_depth_test_enable(depth_test_enable),
        m_depth_write_enable(depth_write_enable),
        m_depth_compare_op(depth_compare_op),
        m_depth_bounds_test_enable(depth_bounds_test_enable),
        m_stencil_test_enable(stencil_test_enable),
        m_front(front),
        m_back(back),
        m_min_depth_bounds(min_depth_bounds),
        m_max_depth_bounds(max_depth_bounds) {}
    DepthStencilStateInfo(const Self & other) noexcept = default;
    DepthStencilStateInfo(Self && other) noexcept = default;
    Self & operator=(const Self & other) noexcept = default;
    Self & operator=(Self && other) noexcept = default;
    operator vk::PipelineDepthStencilStateCreateInfo() const noexcept
    {
        return vk::PipelineDepthStencilStateCreateInfo {
            m_flags,
            m_depth_test_enable, m_depth_write_enable, m_depth_compare_op,
            m_depth_bounds_test_enable,
            m_stencil_test_enable, m_front, m_back,
            m_min_depth_bounds, m_max_depth_bounds,
        };
    }
public:
    Self & addFlags(vk::PipelineDepthStencilStateCreateFlags flags) noexcept { m_flags |= flags; return *this; }
    Self & enableDepthTest(bool write_enable = true, vk::CompareOp compare_op = vk::CompareOp::eLess) noexcept
    {
        m_depth_test_enable = true;
        m_depth_write_enable = write_enable;
        m_depth_compare_op = compare_op;
        return *this;
    }
    Self & enableStencilTest(const vk::StencilOpState & front = {}, const vk::StencilOpState & back = {}) noexcept
    {
        m_stencil_test_enable = true;
        m_front = front;
        m_back = back;
        return *this;
    }
    Self & enableDepthBoundsTest(float min_bounds = 0.0f, float max_bounds = 1.0f) noexcept
    {
        m_depth_bounds_test_enable = true;
        m_min_depth_bounds = min_bounds;
        m_max_depth_bounds = max_bounds;
        return *this;
    }
    const vk::PipelineDepthStencilStateCreateFlags & getFlags() const noexcept { return m_flags; }
    const vk::Bool32 & isDepthTestEnabled() const noexcept { return m_depth_test_enable; }
    const vk::Bool32 & isDepthWriteEnabled() const noexcept { return m_depth_write_enable; }
    const vk::CompareOp & getDepthCompareOp() const noexcept { return m_depth_compare_op; }
    const vk::Bool32 & isDepthBoundsTestEnabled() const noexcept { return m_depth_bounds_test_enable; }
    const vk::Bool32 & isStencilTestEnabled() const noexcept { return m_stencil_test_enable; }
    const vk::StencilOpState & getFrontStencilState() const noexcept { return m_front; }
    const vk::StencilOpState & getBackStencilState() const noexcept { return m_back; }
    const float & getMinDepthBounds() const noexcept { return m_min_depth_bounds; }
    const float & getMaxDepthBounds() const noexcept { return m_max_depth_bounds; }
private:
    vk::PipelineDepthStencilStateCreateFlags m_flags;
    vk::Bool32 m_depth_test_enable;
    vk::Bool32 m_depth_write_enable;
    vk::CompareOp m_depth_compare_op;
    vk::Bool32 m_depth_bounds_test_enable;
    vk::Bool32 m_stencil_test_enable;
    vk::StencilOpState m_front;
    vk::StencilOpState m_back;
    float m_min_depth_bounds;
    float m_max_depth_bounds;
};

class ColorBlendStateInfo
{
    using Self = ColorBlendStateInfo;
    using AttachmentList = std::vector<vk::PipelineColorBlendAttachmentState>;
    using BlendConstants = std::array<float, 4>;
    static constexpr vk::ColorComponentFlags k_rgba =
        vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA;
public:
    ~ColorBlendStateInfo() noexcept = default;
    ColorBlendStateInfo(
        vk::PipelineColorBlendStateCreateFlags flags = {},
        vk::Bool32 logic_op_enable = false,
        vk::LogicOp logic_op = vk::LogicOp::eCopy,
        AttachmentList attachments = {},
        const BlendConstants & blend_constants = {}) noexcept :
        m_flags(flags),
        m_logic_op_enable(logic_op_enable),
        m_logic_op(logic_op),
        m_attachments(std::move(attachments)),
        m_blend_constants(blend_constants) {}
    ColorBlendStateInfo(const Self & other) = default;
    ColorBlendStateInfo(Self && other) noexcept = default;
    Self & operator=(const Self & other) = default;
    Self & operator=(Self && other) noexcept = default;
    operator vk::PipelineColorBlendStateCreateInfo() const noexcept
    {
        return vk::PipelineColorBlendStateCreateInfo {
            m_flags, m_logic_op_enable, m_logic_op
        }.setAttachments(m_attachments).setBlendConstants(m_blend_constants);
    }
public:
    Self & addFlags(vk::PipelineColorBlendStateCreateFlags flags) noexcept { m_flags |= flags; return *this; }
    Self & enableLogicOp(vk::LogicOp logic_op = vk::LogicOp::eCopy) noexcept
    {
        m_logic_op_enable = true;
        m_logic_op = logic_op;
        return *this;
    }
    Self & setBlendConstants(const BlendConstants & constants) noexcept { m_blend_constants = constants; return *this; }
    Self & addAttachment(vk::ColorComponentFlags write_mask = k_rgba)
    {
        m_attachments.emplace_back(vk::PipelineColorBlendAttachmentState{}.setColorWriteMask(write_mask));
        return *this;
    }
    Self & addBlendAttachment(
        vk::BlendFactor src_color, vk::BlendFactor dst_color, vk::BlendOp color_op,
        vk::BlendFactor src_alpha, vk::BlendFactor dst_alpha, vk::BlendOp alpha_op,
        vk::ColorComponentFlags write_mask = k_rgba)
    {
        m_attachments.emplace_back(true, src_color, dst_color, color_op, src_alpha, dst_alpha, alpha_op, write_mask);
        return *this;
    }
    const vk::PipelineColorBlendStateCreateFlags & getFlags() const noexcept { return m_flags; }
    const vk::Bool32 & isLogicOpEnabled() const noexcept { return m_logic_op_enable; }
    const vk::LogicOp & getLogicOp() const noexcept { return m_logic_op; }
    const BlendConstants & getBlendConstants() const noexcept { return m_blend_constants; }
    std::span<const vk::PipelineColorBlendAttachmentState> getAttachments() const noexcept { return m_attachments; }
private:
    vk::PipelineColorBlendStateCreateFlags m_flags;
    vk::Bool32 m_logic_op_enable;
    vk::LogicOp m_logic_op;
    AttachmentList m_attachments;
    BlendConstants m_blend_constants;
};

class ViewportStateInfo
{
    using Self = ViewportStateInfo;
    using ViewportList = std::vector<vk::Viewport>;
    using ScissorList = std::vector<vk::Rect2D>;
public:
    ~ViewportStateInfo() noexcept = default;
    ViewportStateInfo(
        vk::PipelineViewportStateCreateFlags flags = {},
        ViewportList viewports = {},
        ScissorList scissors = {}) noexcept :
        m_flags(flags),
        m_viewports(std::move(viewports)),
        m_scissors(std::move(scissors)) {}
    ViewportStateInfo(const Self & other) = default;
    ViewportStateInfo(Self && other) noexcept = default;
    Self & operator=(const Self & other) = default;
    Self & operator=(Self && other) noexcept = default;
    operator vk::PipelineViewportStateCreateInfo() const noexcept
    {
        return vk::PipelineViewportStateCreateInfo { m_flags }.setViewports(m_viewports).setScissors(m_scissors);
    }
public:
    Self & addFlags(vk::PipelineViewportStateCreateFlags flags) noexcept { m_flags |= flags; return *this; }
    Self & addViewport(float x, float y, float width, float height, float min_depth = 0.0f, float max_depth = 1.0f)
    {
        m_viewports.emplace_back(x, y, width, height, min_depth, max_depth);
        return *this;
    }
    Self & addScissor(int32_t offset_x, int32_t offset_y, uint32_t width, uint32_t height)
    {
        m_scissors.emplace_back(vk::Offset2D{offset_x, offset_y}, vk::Extent2D{width, height});
        return *this;
    }
    const vk::PipelineViewportStateCreateFlags & getFlags() const noexcept { return m_flags; }
    std::span<const vk::Viewport> getViewports() const noexcept { return m_viewports; }
    std::span<const vk::Rect2D> getScissors() const noexcept { return m_scissors; }
private:
    vk::PipelineViewportStateCreateFlags m_flags;
    ViewportList m_viewports;
    ScissorList m_scissors;
};

class DynamicStateInfo
{
    using Self = DynamicStateInfo;
    using StateList = std::vector<vk::DynamicState>;
public:
    ~DynamicStateInfo() noexcept = default;
    DynamicStateInfo(
        vk::PipelineDynamicStateCreateFlags flags = {},
        StateList states = {}) noexcept :
        m_flags(flags),
        m_states(std::move(states)) {}
    DynamicStateInfo(const Self & other) = default;
    DynamicStateInfo(Self && other) noexcept = default;
    Self & operator=(const Self & other) = default;
    Self & operator=(Self && other) noexcept = default;
    operator vk::PipelineDynamicStateCreateInfo() const noexcept
    {
        return vk::PipelineDynamicStateCreateInfo { m_flags }.setDynamicStates(m_states);
    }
public:
    Self & addFlags(vk::PipelineDynamicStateCreateFlags flags) noexcept { m_flags |= flags; return *this; }
    Self & addDynamicState(vk::DynamicState state) { m_states.emplace_back(state); return *this; }
    Self & setDynamicStates(StateList states) { m_states = std::move(states); return *this; }
    const vk::PipelineDynamicStateCreateFlags & getFlags() const noexcept { return m_flags; }
    std::span<const vk::DynamicState> getDynamicStates() const noexcept { return m_states; }
private:
    vk::PipelineDynamicStateCreateFlags m_flags;
    StateList m_states;
};

class GraphicPipelineInfo
{
    using Self = GraphicPipelineInfo;
public:
    ~GraphicPipelineInfo() noexcept = default;
    GraphicPipelineInfo() = default;
    GraphicPipelineInfo(const Self & other) = default;
    GraphicPipelineInfo(Self && other) noexcept = default;
    Self & operator=(const Self & other) = default;
    Self & operator=(Self && other) noexcept = default;
public:
    Self & setShaderProgramInfo(ShaderProgramInfo info) { m_shader_program_info = std::move(info); return *this; }
    Self & setVertexInputInfo(VertexInputInfo info) { m_vertex_input_info_opt = std::move(info); return *this; }
    Self & setInputAssemblyStateInfo(const InputAssemblyStateInfo & info) { m_input_assembly_info = info; return *this; }
    Self & setTessellationStateInfo(const TessellationStateInfo & info) { m_tessellation_info = info; return *this; }
    Self & setRasterizationStateInfo(const RasterizationStateInfo & info) { m_rasterization_info = info; return *this; }
    Self & setMultisampleStateInfo(MultisampleStateInfo info) { m_multisample_info = std::move(info); return *this; }
    Self & setDepthStencilStateInfo(const DepthStencilStateInfo & info) { m_depth_stencil_info = info; return *this; }
    Self & setColorBlendStateInfo(ColorBlendStateInfo info) { m_color_blend_info = std::move(info); return *this; }
    Self & setViewportStateInfo(ViewportStateInfo info) { m_viewport_info = std::move(info); return *this; }
    Self & setDynamicStateInfo(DynamicStateInfo info) { m_dynamic_state_info = std::move(info); return *this; }

    const ShaderProgramInfo & getShaderProgramInfo() const noexcept { return m_shader_program_info; }
    const std::optional<VertexInputInfo> & getVertexInputInfo() const noexcept { return m_vertex_input_info_opt; }
    const InputAssemblyStateInfo & getInputAssemblyStateInfo() const noexcept { return m_input_assembly_info; }
    const TessellationStateInfo & getTessellationStateInfo() const noexcept { return m_tessellation_info; }
    const RasterizationStateInfo & getRasterizationStateInfo() const noexcept { return m_rasterization_info; }
    const MultisampleStateInfo & getMultisampleStateInfo() const noexcept { return m_multisample_info; }
    const DepthStencilStateInfo & getDepthStencilStateInfo() const noexcept { return m_depth_stencil_info; }
    const ColorBlendStateInfo & getColorBlendStateInfo() const noexcept { return m_color_blend_info; }
    const ViewportStateInfo & getViewportStateInfo() const noexcept { return m_viewport_info; }
    const DynamicStateInfo & getDynamicStateInfo() const noexcept { return m_dynamic_state_info; }
private:
    ShaderProgramInfo m_shader_program_info;
    std::optional<VertexInputInfo> m_vertex_input_info_opt;
    InputAssemblyStateInfo m_input_assembly_info;
    TessellationStateInfo m_tessellation_info;
    RasterizationStateInfo m_rasterization_info;
    MultisampleStateInfo m_multisample_info;
    DepthStencilStateInfo m_depth_stencil_info;
    ColorBlendStateInfo m_color_blend_info;
    ViewportStateInfo m_viewport_info;
    DynamicStateInfo m_dynamic_state_info;
};

class RenderingInfo
{
};
/*
vk::AttachmentDescription2;
vk::AttachmentReference2;
vk::SubpassDescription2;
vk::SubpassDependency2;
vk::RenderPassCreateInfo2;
vk::FramebufferCreateInfo;
*/

} // namespace lcf::vkc

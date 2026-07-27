#pragma once

#include <vulkan/vulkan.hpp>
#include <vector>

namespace lcf::vkc {

class GraphicsPipelineInfo;

class CommandBufferProxy;

class StaticRenderScopeInfo;

class DynamicRenderScopeInfo;

class GraphicsPipeline
{
    using Self = GraphicsPipeline;
public:
    ~GraphicsPipeline() noexcept = default;
    GraphicsPipeline() noexcept = default;
    GraphicsPipeline(const Self &) noexcept = delete;
    GraphicsPipeline(Self &&) noexcept = default;
    Self &operator=(const Self &) noexcept = delete;
    Self &operator=(Self &&) noexcept = default;
    operator const vk::Pipeline &() const noexcept { return m_pipeline.get(); }
public:
    std::error_code create(
        vk::Device device,
        const GraphicsPipelineInfo & pipeline_info,
        const StaticRenderScopeInfo & render_scope_info) noexcept;
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
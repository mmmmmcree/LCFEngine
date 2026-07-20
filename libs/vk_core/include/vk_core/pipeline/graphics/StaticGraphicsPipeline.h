#pragma once

#include <vulkan/vulkan.hpp>
#include <vector>

namespace lcf::vkc {

class GraphicsPipelineInfo;

class CommandBufferProxy;

class StaticRendering;

class StaticGraphicsPipeline
{
    using Self = StaticGraphicsPipeline;
public:
    ~StaticGraphicsPipeline() noexcept = default;
    StaticGraphicsPipeline() noexcept = default;
    StaticGraphicsPipeline(const Self &) noexcept = delete;
    StaticGraphicsPipeline(Self &&) noexcept = default;
    Self &operator=(const Self &) noexcept = delete;
    Self &operator=(Self &&) noexcept = default;
    operator const vk::Pipeline &() const noexcept { return m_pipeline.get(); }
public:
    std::error_code create(
        vk::Device device,
        const GraphicsPipelineInfo & pipeline_info,
        const StaticRendering & static_rendering, uint32_t subpass_index = 0) noexcept;
    void bind(CommandBufferProxy & cmd) const noexcept;
    const vk::Pipeline & handle() const noexcept { return m_pipeline.get(); }
private:
    vk::UniquePipeline m_pipeline;
    vk::UniquePipelineLayout m_pipeline_layout;
    std::vector<vk::UniqueShaderModule> m_shader_modules;
};

} // namespace lcf::vkc
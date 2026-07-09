#pragma once

#include <vulkan/vulkan.hpp>
#include <vector>

namespace lcf::vkc {

class GraphicsPipelineInfo;

class CommandBufferProxy;

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
public:
    std::error_code create(vk::Device device, const GraphicsPipelineInfo & pipeline_info) noexcept;
    void bind(CommandBufferProxy & cmd) const noexcept;
private:
    vk::UniquePipeline m_pipeline;
    vk::UniquePipelineLayout m_pipeline_layout;
    std::vector<vk::UniqueShaderModule> m_shader_modules;
};

} // namespace lcf::vkc
#pragma once

#include <vulkan/vulkan.hpp>

namespace lcf::vkc {

class GraphicPipelineInfo;

class CommandBufferProxy;

class StaticGraphicPipeline
{
    using Self = StaticGraphicPipeline;
public:
    ~StaticGraphicPipeline() noexcept = default;
    StaticGraphicPipeline() noexcept = default;
    StaticGraphicPipeline(const Self &) noexcept = delete;
    StaticGraphicPipeline(Self &&) noexcept = default;
    Self &operator=(const Self &) noexcept = delete;
    Self &operator=(Self &&) noexcept = default;
public:
    std::error_code create(vk::Device device, const GraphicPipelineInfo & pipeline_info) noexcept;
    void bind(CommandBufferProxy & cmd) const noexcept;
private:
    vk::UniquePipeline m_pipeline;
};

} // namespace lcf::vkc
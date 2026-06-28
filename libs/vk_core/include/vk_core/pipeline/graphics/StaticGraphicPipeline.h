#pragma once

#include <vulkan/vulkan.hpp>

namespace lcf::vkc {

class GraphicPipelineInfo;

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
    std::error_code create(vk::Device device, const GraphicPipelineInfo &info) noexcept;
    void bind(vk::CommandBuffer cmd) noexcept;
private:
    vk::UniquePipeline m_pipeline;
};

} // namespace lcf::vkc
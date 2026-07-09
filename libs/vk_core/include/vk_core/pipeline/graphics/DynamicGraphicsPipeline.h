#pragma once

#include <vulkan/vulkan.hpp>

namespace lcf::vkc {

class GraphicsPipelineInfo;

class DynamicGraphicPipeline
{
    using Self = DynamicGraphicPipeline;
public:
    ~DynamicGraphicPipeline() noexcept = default;
    DynamicGraphicPipeline() noexcept = default;
    DynamicGraphicPipeline(const Self &) noexcept = delete;
    DynamicGraphicPipeline(Self &&) noexcept = default;
    Self &operator=(const Self &) noexcept = delete;
    Self &operator=(Self &&) noexcept = default;
public:
    std::error_code create(vk::Device device, const GraphicsPipelineInfo &info) noexcept;
    void bind(vk::CommandBuffer cmd) noexcept;
private:
    vk::UniquePipeline m_pipeline;
};

} // namespace lcf::vkc
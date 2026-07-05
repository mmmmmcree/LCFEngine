#include "vk_core/pipeline/graphics/StaticGraphicPipeline.h"
#include "vk_core/pipeline/graphics/info_structs.h"
#include "vk_core/command/CommandBufferProxy.h"

namespace lcf::vkc {

std::error_code StaticGraphicPipeline::create(vk::Device device, const GraphicPipelineInfo &pipeline_info) noexcept
{
    return {};
}

void StaticGraphicPipeline::bind(CommandBufferProxy & cmd) const noexcept
{
    cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, m_pipeline.get());
}

} // namespace lcf::vkc

#include "vulkan_utililtie.h"

using namespace lcf::render::vkutils;

void lcf::render::vkutils::immediate_submit(VulkanContext *context, vk::QueueFlagBits queue_type, std::function<void(VulkanCommandBufferObject &)> &&submit_func)
{
    auto device = context->getDevice();
    VulkanCommandBufferObject cmd;
    cmd.create(context, queue_type);
    vk::CommandBufferBeginInfo cmd_begin_info;
    cmd_begin_info.setFlags(vk::CommandBufferUsageFlagBits::eOneTimeSubmit);
    cmd.begin(cmd_begin_info);
    submit_func(cmd);
    cmd.end();
    cmd.submit();
    cmd.getTimelineSemaphore()->wait();
}

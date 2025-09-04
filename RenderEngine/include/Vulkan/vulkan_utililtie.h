#pragma once

#include <vulkan/vulkan.hpp>
#include <functional>
#include "VulkanContext.h"

namespace lcf::render::vkutils {
    class ImageLayoutTransitionAssistant : public vk::ImageMemoryBarrier2
    {
    public:
        ImageLayoutTransitionAssistant();
        void setImage(vk::Image image, vk::ImageLayout current_layout = vk::ImageLayout::eUndefined);
        void transitTo(vk::CommandBuffer cmd, vk::ImageLayout new_layout);
    private:
        vk::Image m_image = nullptr;
    };

    class CopyAssistant
    {
    public:
        CopyAssistant(vk::CommandBuffer cmd);
        void copy(vk::Image src, vk::Image dst, vk::Extent2D src_size, vk::Extent2D dst_size, vk::Filter filter = vk::Filter::eNearest);
    private:
        vk::CommandBuffer m_cmd = nullptr;
    };

    void immediate_submit(VulkanContext * context, std::function<void()> && submit_func);
}
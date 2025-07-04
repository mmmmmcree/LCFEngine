#pragma once

#include "RHI/RenderExchangeInfo.h"
#include <vulkan/vulkan.hpp>

namespace lcf::render {
    class VulkanRenderExchangeInfo : public RenderExchangeInfo
    {
        using Self = VulkanRenderExchangeInfo;
    public:
        VulkanRenderExchangeInfo() = default;
        Self & setTargetAvailableSemaphore(vk::Semaphore semaphore) { target_available = semaphore; return *this; }
        Self & setRenderFinishedSemaphore(vk::Semaphore semaphore) { render_finished = semaphore; return *this; }
        Self & setTargetImage(vk::Image image) { target_image = image; return *this; }
        Self & setTargetImageView(vk::ImageView view) { target_image_view = view; return *this; }
        vk::Semaphore getTargetAvailableSemaphore() const { return target_available; }
        vk::Semaphore getRenderFinishedSemaphore() const { return render_finished; }
        vk::Image getTargetImage() const { return target_image; }
        vk::ImageView getTargetImageView() const { return target_image_view; }
    private:
        vk::Semaphore target_available;
        vk::Semaphore render_finished;
        vk::Image target_image;
        vk::ImageView target_image_view;
    };
}
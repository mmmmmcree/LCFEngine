#pragma once

#include "RHI/RenderTarget.h"
#include <vulkan/vulkan.hpp>

namespace lcf::render {
    class VulkanContext;
    class VulkanSwapchain : public RenderTarget, public PointerDefs<VulkanSwapchain>
    {
    public:
        IMPORT_POINTER_DEFS(VulkanSwapchain);
        VulkanSwapchain(VulkanContext * context, vk::SurfaceKHR surface);
        virtual ~VulkanSwapchain() override;
        void create() override;
        bool isCreated() override { return m_swapchain.get(); }
        void recreate();
        void destroy() override;
        bool isValid() const override { return m_surface and m_swapchain.get(); }
        uint32_t getWidth() const override { return m_surface_capabilities.currentExtent.width; }
        uint32_t getHeight() const override { return m_surface_capabilities.currentExtent.height; }
        // bool prepareForRender(RenderExchangeInfo * exchange_info) override;
        bool prepareForRender();
        vk::Image getTargetImage() const { return m_swapchain_resources_list[m_image_index].image; }
        vk::Semaphore getTargetAvailableSemaphore() const { return m_swapchain_resources_list[m_target_available_index].target_available.get(); }
        vk::ImageLayout getTargetImageLayout() const { return vk::ImageLayout::ePresentSrcKHR; }
        void finishRender(vk::Semaphore present_ready);
        void present();
        vk::SwapchainKHR getHandle() const { return m_swapchain.get(); }
        vk::SurfaceKHR getSurface() const { return m_surface.get(); }
        vk::Extent2D getExtent() const { return m_surface_capabilities.currentExtent; }
    private:
        void acquireAvailableTarget();
    private:
        inline static constexpr uint32_t SWAPCHAIN_BUFFER_COUNT = 4;
        VulkanContext * m_context = nullptr;
        vk::UniqueSwapchainKHR m_swapchain;
        // vk::SurfaceKHR m_surface;
        vk::UniqueSurfaceKHR m_surface;
        vk::SurfaceFormatKHR m_surface_format;
        vk::PresentModeKHR  m_present_mode;
        vk::SurfaceCapabilitiesKHR m_surface_capabilities;
        struct SwapchainResources
        {
            SwapchainResources() = default;
            vk::Image image;
            vk::UniqueImageView image_view;
            vk::UniqueSemaphore target_available;
        };
        std::array<SwapchainResources, SWAPCHAIN_BUFFER_COUNT> m_swapchain_resources_list;
        uint32_t m_image_index = 0; // assigned by vkAcquireNextImageKHR
        uint32_t m_target_available_index = 0; // unrelated to m_image_index
        vk::Semaphore m_present_ready;
    };
}
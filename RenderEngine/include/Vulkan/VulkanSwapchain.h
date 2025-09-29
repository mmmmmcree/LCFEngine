#pragma once

#include "common/RenderTarget.h"
#include <vulkan/vulkan.hpp>
#include "VulkanImage.h"

namespace lcf::render {
    class VulkanContext;
    class VulkanSwapchain : public RenderTarget, public STDPointerDefs<VulkanSwapchain>
    {
    public:
        IMPORT_POINTER_DEFS(STDPointerDefs<VulkanSwapchain>);
        VulkanSwapchain(VulkanContext * context, vk::SurfaceKHR surface);
        VulkanSwapchain(const VulkanSwapchain &) = delete;
        VulkanSwapchain & operator=(const VulkanSwapchain &) = delete;
        virtual ~VulkanSwapchain() override;
        void create() override;
        bool isCreated() override { return m_swapchain.get(); }
        void recreate();
        void destroy() override;
        bool isValid() const override { return m_surface and m_swapchain.get(); }
        bool prepareForRender();
        const VulkanImage::SharedPointer & getTargetImageSharedPointer() const noexcept { return m_swapchain_resources_list[m_image_index].image_sp; }
        vk::Semaphore getTargetAvailableSemaphore() const { return m_swapchain_resources_list[m_target_available_index].target_available.get(); }
        void finishRender(vk::Semaphore present_ready);
        void present();
        vk::SwapchainKHR getHandle() const { return m_swapchain.get(); }
        vk::SurfaceKHR getSurface() const { return m_surface; }
        vk::Extent2D getExtent() const { return m_surface_capabilities.currentExtent; }
    private:
        void acquireAvailableTarget();
    private:
        inline static constexpr uint32_t SWAPCHAIN_BUFFER_COUNT = 4;
        VulkanContext * m_context_p = nullptr;
        vk::UniqueSwapchainKHR m_swapchain;
        vk::SurfaceKHR m_surface;
        vk::SurfaceFormatKHR m_surface_format;
        vk::PresentModeKHR  m_present_mode;
        vk::SurfaceCapabilitiesKHR m_surface_capabilities;
        struct SwapchainResources
        {
            SwapchainResources() = default;
            VulkanImage::SharedPointer image_sp;
            vk::UniqueSemaphore target_available;
        };
        std::array<SwapchainResources, SWAPCHAIN_BUFFER_COUNT> m_swapchain_resources_list;
        uint32_t m_image_index = 0; // assigned by vkAcquireNextImageKHR
        uint32_t m_target_available_index = 0; // unrelated to m_image_index
        vk::Semaphore m_present_ready;
    };
}
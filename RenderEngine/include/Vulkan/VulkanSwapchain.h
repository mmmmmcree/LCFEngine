#pragma once

#include "common/RenderTarget.h"
#include <vulkan/vulkan.hpp>
#include "VulkanImage.h"
#include <optional>
#include <queue>

namespace lcf::render {
    class VulkanContext;

    class VulkanSwapchain : public RenderTarget, public STDPointerDefs<VulkanSwapchain>
    {
    public:
        IMPORT_POINTER_DEFS(STDPointerDefs<VulkanSwapchain>);
        struct FrameResources;
        struct PendingRecycleResources;
        using OptionalFrameResources = std::optional<FrameResources>;
        using FencePool = std::queue<vk::UniqueFence>;
        using SemaphorePool = std::queue<vk::UniqueSemaphore>;
        using PendingRecycleResourcesQueue = std::queue<PendingRecycleResources>;
        VulkanSwapchain(vk::SurfaceKHR surface);
        ~VulkanSwapchain();
        void create(VulkanContext * context_p);
        bool isCreated() const noexcept { return m_swapchain.get(); }
        bool isValid() const noexcept { return m_surface and m_swapchain.get(); }
        bool prepareForRender();
        bool finishRender();
        const FrameResources & getCurrentFrameResources() const noexcept { return *m_current_frame_resources; }
        vk::SwapchainKHR getHandle() const noexcept { return m_swapchain.get(); }
        vk::SurfaceKHR getSurface() const noexcept { return m_surface; }
        vk::Extent2D getExtent() const noexcept { return m_surface_capabilities.currentExtent; }
    private:
        bool recreate();
        bool acquireAvailableTarget();
        bool present();
        void collectCurrentFrameResources();
        void collectCurrentPendingResources();
        void tryRecycle();
        vk::UniqueFence acquireFence();
        vk::UniqueSemaphore acquireSemaphore();
    private:
        struct FrameResources
        {
            FrameResources() = default;
            FrameResources(const FrameResources & other) = delete;
            FrameResources & operator=(const FrameResources & other) = delete;
            FrameResources(FrameResources && other) noexcept;
            FrameResources & operator=(FrameResources && other) noexcept;
            void setImage(const VulkanImage::SharedPointer & image_sp) noexcept { m_image_sp = image_sp; }
            void setTargetAvailableSemaphore(vk::UniqueSemaphore && semaphore) noexcept { m_target_available = std::move(semaphore); }
            void setPresentReadySemaphore(vk::UniqueSemaphore && semaphore) noexcept { m_present_ready = std::move(semaphore); }
            const VulkanImage::SharedPointer getImageSharedPointer() const noexcept { return m_image_sp; }
            const vk::Semaphore & getTargetAvailableSemaphore() const noexcept { return m_target_available.get(); }
            const vk::Semaphore & getPresentReadySemaphore() const noexcept { return m_present_ready.get(); }
            VulkanImage::SharedPointer m_image_sp;
            vk::UniqueSemaphore m_target_available;
            vk::UniqueSemaphore m_present_ready;
        };
        struct PendingRecycleResources
        {
            PendingRecycleResources() = default;
            PendingRecycleResources(const PendingRecycleResources & other) = delete;
            PendingRecycleResources & operator=(const PendingRecycleResources & other) = delete;
            PendingRecycleResources(PendingRecycleResources && other) noexcept;
            PendingRecycleResources & operator=(PendingRecycleResources && other) noexcept;
            void setPresentFence(vk::UniqueFence && fence) noexcept { m_present_fence = std::move(fence); }
            void collect(FrameResources && resources) noexcept;
            void collect(VulkanImage::SharedPointer && image_sp) noexcept { m_images.emplace_back(std::move(image_sp)); }
            void collect(vk::UniqueSwapchainKHR && swapchain) noexcept { m_swapchains.emplace_back(std::move(swapchain)); }
            vk::UniqueFence m_present_fence;
            std::vector<vk::UniqueSemaphore> m_semaphores;
            std::vector<VulkanImage::SharedPointer> m_images;
            std::vector<vk::UniqueSwapchainKHR> m_swapchains;
        };
    private:
        inline static constexpr uint32_t SWAPCHAIN_BUFFER_COUNT = 4;
        VulkanContext * m_context_p = nullptr;
        vk::UniqueSwapchainKHR m_swapchain;
        vk::SurfaceKHR m_surface;
        vk::SurfaceFormatKHR m_surface_format;
        vk::PresentModeKHR  m_present_mode;
        vk::SurfaceCapabilitiesKHR m_surface_capabilities;
        std::array<VulkanImage::SharedPointer, SWAPCHAIN_BUFFER_COUNT> m_swapchain_images;
        uint32_t m_image_index = 0; // assigned by vkAcquireNextImageKHR
        OptionalFrameResources m_current_frame_resources;
        PendingRecycleResources m_current_pending_resources;
        FencePool m_fence_pool;
        SemaphorePool m_semaphore_pool;
        PendingRecycleResourcesQueue m_recycle_queue;
    };
}
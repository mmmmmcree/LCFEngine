#pragma once

#include <vulkan/vulkan.hpp>
#include "WindowHandle.h"
#include "resource_utils.h"
#include <array>
#include <vector>
#include <queue>
#include <expected>

namespace lcf::vkc {

class RenderDeviceContext;

namespace wsi {

class Swapchain
{
    using ImageList = std::vector<vk::Image>;
    struct PresentResources
    {
        vk::CommandBuffer m_cmd = nullptr;
        vk::UniqueSemaphore m_target_available;
        vk::UniqueSemaphore m_present_ready;
        vk::UniqueFence m_present_fence;
        std::vector<ResourceLease> m_leases;
    };
    using CmdBufferPool = std::queue<vk::CommandBuffer>;
    using FencePool = std::queue<vk::UniqueFence>;
    using SemaphorePool = std::queue<vk::UniqueSemaphore>;
    using PendingRecycleResourcesQueue = std::queue<PresentResources>;
public:
    Swapchain() noexcept = default;
    std::error_code create(
        vk::Instance instance,
        RenderDeviceContext & device_context,
        const WindowHandle & window_handle,
        uint32_t swapchain_image_count) noexcept;
    std::error_code present(
        vk::SemaphoreSubmitInfo wait_info,
        vk::Image src_image,
        ResourceLease image_lease,
        const std::array<vk::Offset3D, 2> & src_offsets,
        vk::ImageSubresourceLayers src_subresource_layers = {vk::ImageAspectFlagBits::eColor, 0u, 0u, 1u}) noexcept;
    std::error_code present(
        vk::Image src_image,
        ResourceLease image_lease,
        const std::array<vk::Offset3D, 2> & src_offsets,
        vk::ImageSubresourceLayers src_subresource_layers = {vk::ImageAspectFlagBits::eColor, 0u, 0u, 1u}) noexcept;
    std::error_code present(
        vk::Image src_image,
        const std::array<vk::Offset3D, 2> & src_offsets,
        vk::ImageSubresourceLayers src_subresource_layers = {vk::ImageAspectFlagBits::eColor, 0u, 0u, 1u}) noexcept;
private:
    std::error_code recreate() noexcept;
    std::error_code acquireNextImage() noexcept;
    std::expected<vk::CommandBuffer, std::error_code> acquireCmdBuffer() noexcept;
    std::expected<vk::UniqueFence, std::error_code> acquireFence() noexcept;
    std::expected<vk::UniqueSemaphore, std::error_code> acquireSemaphore() noexcept;
    void recyclePresentResources(PresentResources & present_resources) noexcept;
    void tryRecyclePendingResources() noexcept;
private:
    RenderDeviceContext * m_device_context_p;
    vk::UniqueSurfaceKHR m_surface;
    vk::UniqueSwapchainKHR m_swapchain;
    uint32_t m_width = 0u, m_height = 0u;
    vk::SurfaceFormatKHR m_surface_format;
    vk::PresentModeKHR  m_present_mode;
    ImageList m_swapchain_images;
    uint32_t m_image_index = 0u;
    PresentResources m_present_resources;
    PendingRecycleResourcesQueue m_pending_resources_queue;
    vk::UniqueCommandPool m_cmd_pool;
    CmdBufferPool m_cmd_buffer_pool;
    FencePool m_fence_pool;
    SemaphorePool m_semaphore_pool;
};

} // namespace lcf::vkc::wsi

} // namespace lcf::vkc
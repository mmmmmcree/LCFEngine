#pragma once

#include <vulkan/vulkan.hpp>
#include "vk_core/WSI/WindowHandle.h"
#include "resource_utils.h"
#include "AtomicSnapshot.h"
#include "SPSCValue.h"
#include <array>
#include <vector>
#include <queue>
#include <expected>
#include <atomic>
#include <mutex>

namespace lcf::vkc {
namespace wsi::compat {

class Swapchain
{
    using Self = Swapchain;
    using ImageList = std::vector<vk::Image>;
    struct DesiredParams
    {
        bool operator==(const DesiredParams & other) const noexcept = default;
        uint32_t image_count = 4u;
        vk::SurfaceFormatKHR surface_format = {vk::Format::eB8G8R8A8Srgb, vk::ColorSpaceKHR::eSrgbNonlinear};
        vk::PresentModeKHR present_mode = vk::PresentModeKHR::eMailbox;
    };
    struct PresentResources
    {
        vk::CommandBuffer m_cmd = nullptr;
        vk::UniqueSemaphore m_target_available;
        vk::UniqueFence m_submit_fence;
        std::vector<ResourceLease> m_leases;
    };
    struct CachedPresentInput
    {
        std::array<vk::Offset3D, 2> m_src_offsets;
        vk::Image m_src_image = nullptr;
        ResourceLease m_src_lease;
        vk::SemaphoreSubmitInfo wait_info;
        vk::ImageSubresourceLayers m_src_subresource_layers;
    };
    using CmdBufferPool = std::queue<vk::CommandBuffer>;
    using FencePool = std::queue<vk::UniqueFence>;
    using SemaphorePool = std::queue<vk::UniqueSemaphore>;
    using PendingResourcesQueue = std::queue<PresentResources>;
    using SemaphoreList = std::vector<vk::UniqueSemaphore>;
public:
    ~Swapchain() noexcept = default;
    Swapchain() noexcept = default;
    Swapchain(const Self &) = delete;
    Swapchain(Swapchain &&) noexcept = delete;
    Swapchain & operator=(const Self &) = delete;
    Swapchain & operator=(Swapchain &&) noexcept = delete;
public:
    std::error_code create(
        vk::Instance instance,
        vk::PhysicalDevice physical_device,
        vk::Device device,
        uint32_t present_queue_family_index,
        const WindowHandle & window_handle) noexcept;
    std::error_code present(
        const std::array<vk::Offset3D, 2> & src_offsets,
        vk::Image src_image,
        ResourceLease image_lease = {},
        vk::SemaphoreSubmitInfo wait_info = {},
        vk::ImageSubresourceLayers src_subresource_layers = {vk::ImageAspectFlagBits::eColor, 0u, 0u, 1u}) noexcept;
    std::error_code resizeToFit() noexcept;
    Self & setDesiredSwapchainImageCount(uint32_t desired_count) noexcept;
    Self & setDesiredSurfaceFormat(const vk::SurfaceFormatKHR & surface_format) noexcept;
    Self & setDesiredPresentMode(const vk::PresentModeKHR & present_mode) noexcept;
private:
    std::error_code _present(
        const std::array<vk::Offset3D, 2> & src_offsets,
        vk::Image src_image,
        ResourceLease image_lease,
        vk::SemaphoreSubmitInfo wait_info,
        vk::ImageSubresourceLayers src_subresource_layers) noexcept;
    std::error_code recreate(const DesiredParams & desired_params) noexcept;
    std::error_code acquireNextImage() noexcept;
    std::expected<vk::CommandBuffer, std::error_code> acquireCmdBuffer() noexcept;
    std::expected<vk::UniqueFence, std::error_code> acquireFence() noexcept;
    std::expected<vk::UniqueSemaphore, std::error_code> acquireSemaphore() noexcept;
    void recyclePresentResources(PresentResources & present_resources) noexcept;
    void tryRecyclePendingResources() noexcept;
private:
    vk::PhysicalDevice m_physical_device;
    vk::Device m_device;
    vk::Queue m_present_queue;
    vk::UniqueSurfaceKHR m_surface;
    vk::UniqueSwapchainKHR m_swapchain;
    ImageList m_swapchain_images;
    vk::SurfaceFormatKHR m_surface_format;
    vk::PresentModeKHR m_present_mode;
    uint32_t m_width = 0u, m_height = 0u;
    uint32_t m_image_index = 0u;
    SPSCValue<CachedPresentInput> m_cached_present_input;
    LatchedSnapshot<DesiredParams> m_desired_params_snapshot;
    SemaphoreList m_present_ready_semaphores;
    std::mutex m_present_mutex;
    std::atomic<bool> m_resize_has_priority = false;
    vk::UniqueCommandPool m_cmd_pool;
    CmdBufferPool m_cmd_buffer_pool;
    FencePool m_fence_pool;
    SemaphorePool m_semaphore_pool;
    PresentResources m_present_resources;
    PendingResourcesQueue m_pending_resources_queue;
};

} // namespace lcf::vkc::wsi::compat

} // namespace lcf::vkc
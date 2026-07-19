#pragma once

#include <vulkan/vulkan.hpp>
#include "vk_core/sync/TimelineSemaphore.h"
#include "vk_core/queue/LogicalQueue.h"
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

namespace wsi {

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
        vk::UniqueSemaphore m_present_ready;
        vk::UniqueFence m_present_fence;
        std::vector<ResourceLease> m_leases;
    };
    struct CachedPresentInput
    {
        std::array<vk::Offset3D, 2> m_src_offsets;
        vk::Image m_src_image;
        ResourceLease m_src_lease;
        vk::SemaphoreSubmitInfo m_wait_info;
        vk::ImageSubresourceLayers m_src_subresource_layers;
    };
    using ConstDesiredParamsSP = std::shared_ptr<const DesiredParams>;
    using CmdBufferPool = std::queue<vk::CommandBuffer>;
    using FencePool = std::queue<vk::UniqueFence>;
    using SemaphorePool = std::queue<vk::UniqueSemaphore>;
    using PendingRecycleResourcesQueue = std::queue<PresentResources>;
public:
    ~Swapchain() noexcept = default;
    Swapchain() noexcept = default;
    Swapchain(const Self &) = delete;
    Swapchain(Swapchain &&) noexcept = delete;
    Swapchain & operator=(const Self &) = delete;
    Swapchain & operator=(Swapchain &&) noexcept = delete;
public:
    std::error_code create(
        vk::UniqueSurfaceKHR surface,
        vk::PhysicalDevice physical_device,
        const LogicalQueue & present_queue) noexcept;
    std::expected<vk::SemaphoreSubmitInfo, std::error_code> present(
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
    std::expected<vk::SemaphoreSubmitInfo, std::error_code> _present(
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
    LogicalQueue m_logical_present_queue;
    vk::UniqueSurfaceKHR m_surface;
    vk::UniqueSwapchainKHR m_swapchain;
    ImageList m_swapchain_images;
    vk::SurfaceFormatKHR m_surface_format;
    vk::PresentModeKHR m_present_mode;
    uint32_t m_width = 0u, m_height = 0u;
    uint32_t m_image_index = 0u;
    LatchedSnapshot<DesiredParams> m_desired_params_snapshot;
    SPSCValue<CachedPresentInput> m_cached_present_input;
    std::mutex m_present_mutex;
    std::atomic<bool> m_resize_has_priority = false;
    TimelineSemaphore m_blit_timeline;
    vk::UniqueCommandPool m_cmd_pool;
    CmdBufferPool m_cmd_buffer_pool;
    FencePool m_fence_pool;
    SemaphorePool m_semaphore_pool;
    PresentResources m_present_resources;
    PendingRecycleResourcesQueue m_pending_resources_queue;
};

} // namespace lcf::vkc::wsi

} // namespace lcf::vkc
#pragma once

#include <vulkan/vulkan.hpp>
#include "vk_core/WSI/WindowHandle.h"
#include "resource_utils.h"
#include <array>
#include <vector>
#include <queue>
#include <expected>
#include <atomic>
#include <memory>

namespace lcf::vkc {

class RenderDeviceContext;

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
        vk::UniqueSemaphore m_present_ready;
        vk::UniqueFence m_present_fence;
        std::vector<ResourceLease> m_leases;
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
        vk::Instance instance,
        RenderDeviceContext & device_context,
        const WindowHandle & window_handle) noexcept;
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
    Self & setDesiredSwapchainImageCount(uint32_t desired_count) noexcept;
    Self & setDesiredSurfaceFormat(const vk::SurfaceFormatKHR & surface_format) noexcept;
    Self & setDesiredPresentMode(const vk::PresentModeKHR & present_mode) noexcept;
private:
    std::error_code recreate(const DesiredParams & desired_params) noexcept;
    std::error_code acquireNextImage() noexcept;
    template <typename Mutator>
    Self & updateDesired(Mutator && mutator) noexcept;
    std::expected<vk::CommandBuffer, std::error_code> acquireCmdBuffer() noexcept;
    std::expected<vk::UniqueFence, std::error_code> acquireFence() noexcept;
    std::expected<vk::UniqueSemaphore, std::error_code> acquireSemaphore() noexcept;
    void recyclePresentResources(PresentResources & present_resources) noexcept;
    void tryRecyclePendingResources() noexcept;
private:
    RenderDeviceContext * m_device_context_p;
    vk::UniqueSurfaceKHR m_surface;
    vk::UniqueSwapchainKHR m_swapchain;
    std::atomic<ConstDesiredParamsSP> m_provided_desired_params_asp = std::make_shared<const DesiredParams>();
    ConstDesiredParamsSP m_consumed_desired_params_sp;
    vk::SurfaceFormatKHR m_surface_format;
    vk::PresentModeKHR m_present_mode;
    uint32_t m_width = 0u, m_height = 0u;
    uint32_t m_image_index = 0u;
    ImageList m_swapchain_images;
    PresentResources m_present_resources;
    PendingRecycleResourcesQueue m_pending_resources_queue;
    vk::UniqueCommandPool m_cmd_pool;
    CmdBufferPool m_cmd_buffer_pool;
    FencePool m_fence_pool;
    SemaphorePool m_semaphore_pool;
};

} // namespace lcf::vkc::wsi::compat

} // namespace lcf::vkc
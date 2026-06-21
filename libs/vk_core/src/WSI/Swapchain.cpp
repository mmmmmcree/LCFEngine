#include "vk_core/WSI/Swapchain.h"
#include "vk_core/WSI/create_surface.h"
#include "vk_core/WSI/WindowHandle.h"
#include "vk_core/context/RenderDeviceContext.h"
#include "vk_core/context/QueueContext.h"
#include "vk_core/error.h"
#include <limits>

namespace stdr = std::ranges;

namespace lcf::vkc::wsi {

std::error_code Swapchain::create(
    vk::Instance instance,
    RenderDeviceContext &device_context,
    const WindowHandle &window_handle,
    uint32_t swapchain_image_count) noexcept
{
    m_device_context_p = &device_context;
    auto expected_surface = create_surface(instance, window_handle);
    if (not expected_surface) { return expected_surface.error(); }
    m_surface = std::move(expected_surface.value());
    vk::PhysicalDevice physical_device = device_context.getPhysicalDevice();
    uint32_t present_family_index = device_context.getGraphicsQueueContext().getFamilyIndex();
    bool supported = false;   
    try {
        supported = physical_device.getSurfaceSupportKHR(present_family_index, m_surface.get());
    } catch (const vk::SystemError & e) { return e.code(); }
    if (not supported) { return errc::no_suitable_present_queue_family; }
    std::vector<vk::SurfaceFormatKHR> surface_formats;
    std::vector<vk::PresentModeKHR> present_modes;
    vk::SurfaceCapabilitiesKHR surface_capabilities;
    vk::CommandPoolCreateInfo pool_info {vk::CommandPoolCreateFlagBits::eResetCommandBuffer, present_family_index };
    vk::Device device = device_context.getDevice();
    try {
        surface_formats = physical_device.getSurfaceFormatsKHR(m_surface.get());
        present_modes = physical_device.getSurfacePresentModesKHR(m_surface.get());
        surface_capabilities = physical_device.getSurfaceCapabilitiesKHR(m_surface.get());
        m_cmd_pool = device.createCommandPoolUnique(pool_info);
    } catch (vk::SystemError &e) {
        return e.code();
    }
    auto surface_it = stdr::find_if(surface_formats, [](const auto& f) {
        return f == vk::SurfaceFormatKHR{vk::Format::eB8G8R8A8Srgb, vk::ColorSpaceKHR::eSrgbNonlinear};
    });
    auto present_it = stdr::find_if(present_modes, [](const auto& m) { return m == vk::PresentModeKHR::eMailbox; });
    m_surface_format = surface_it != surface_formats.end() ? *surface_it : surface_formats.front();
    m_present_mode = present_it != present_modes.end() ? *present_it : vk::PresentModeKHR::eFifo;   
    m_swapchain_images.resize(swapchain_image_count);
    return {};
}

std::error_code Swapchain::present(
    vk::SemaphoreSubmitInfo wait_info,
    vk::Image src_image,
    ResourceLease image_lease,
    const std::array<vk::Offset3D, 2> &src_offsets,
    vk::ImageSubresourceLayers src_subresource_layers) noexcept
{
    if (not m_swapchain) { if (auto ec = this->recreate()) { return ec; } }
    if (auto ec = this->acquireNextImage()) { return ec; }
    if (m_width == 0 or m_height == 0) { return {}; }

    auto device = m_device_context_p->getDevice();
    vk::StructureChain<vk::PresentInfoKHR, vk::SwapchainPresentFenceInfoKHR> present_info;
    // todo fill present_info
   
    QueueContext & present_queue_context = m_device_context_p->getGraphicsQueueContext();
    vk::Queue present_queue = present_queue_context.getQueue(); 
    vk::Result present_result = vk::Result::eErrorUnknown;   
    try {
        present_result = present_queue.presentKHR(present_info.get<vk::PresentInfoKHR>());
    } catch (const vk::OutOfDateKHRError &e) {
        //- no need to recreate, recreation will happen in acquireNextImage
    } catch (const vk::SystemError &e) {
        return static_cast<vk::Result>(e.code().value());   
    }
    if (present_result == vk::Result::eSuccess or present_result == vk::Result::eSuboptimalKHR) { return {}; }
    //collect
    return vk::make_error_code(present_result);
}

std::error_code Swapchain::present(
    vk::Image src_image,
    ResourceLease image_lease,
    const std::array<vk::Offset3D, 2> &src_offsets,
    vk::ImageSubresourceLayers src_subresource_layers) noexcept
{
    return this->present({}, src_image, image_lease, src_offsets, src_subresource_layers);
}

std::error_code Swapchain::present(
    vk::Image src_image,
    const std::array<vk::Offset3D, 2> &src_offsets,
    vk::ImageSubresourceLayers src_subresource_layers) noexcept
{
    return this->present({}, src_image, {}, src_offsets, src_subresource_layers);
}

std::error_code Swapchain::recreate() noexcept
{
    vk::PhysicalDevice physical_device = m_device_context_p->getPhysicalDevice();
    vk::SurfaceCapabilitiesKHR surface_capabilities;
    try {
        surface_capabilities = physical_device.getSurfaceCapabilitiesKHR(m_surface.get());
    } catch (vk::SystemError &e) {
        return e.code();
    }
    auto [cur_width, cur_height] = surface_capabilities.currentExtent;
    m_width = cur_width;
    m_height = cur_height;
    if (m_width == 0 or m_height == 0) { return {}; };

    vk::CompositeAlphaFlagBitsKHR composite_bit = vk::CompositeAlphaFlagBitsKHR::eOpaque;
    while (composite_bit & vk::FlagTraits<vk::CompositeAlphaFlagBitsKHR>::allFlags) {
        using MaskType = typename vk::CompositeAlphaFlagsKHR::MaskType;
        if (surface_capabilities.supportedCompositeAlpha & composite_bit) { break; }
        composite_bit = vk::CompositeAlphaFlagBitsKHR(MaskType(composite_bit) << 1);
    }

    vk::Device device = m_device_context_p->getDevice();
    vk::UniqueSwapchainKHR new_swapchain;
    std::vector<vk::Image> swapchain_images;
    vk::SwapchainCreateInfoKHR swapchain_info;
    uint32_t image_count = std::clamp(static_cast<uint32_t>(m_swapchain_images.size()),
        surface_capabilities.minImageCount,
        surface_capabilities.maxImageCount);
    
    swapchain_info.setSurface(m_surface.get())
        .setMinImageCount(image_count)
        .setImageFormat(m_surface_format.format)
        .setImageColorSpace(m_surface_format.colorSpace)
        .setImageExtent({ cur_width, cur_height })
        .setImageArrayLayers(1)
        .setImageUsage(vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eTransferDst)
        .setImageSharingMode(vk::SharingMode::eExclusive)
        .setPreTransform(surface_capabilities.currentTransform)
        .setCompositeAlpha(composite_bit)
        .setPresentMode(m_present_mode)
        .setClipped(true)
        .setOldSwapchain(m_swapchain.get());
    try {
        new_swapchain = device.createSwapchainKHRUnique(swapchain_info);
        swapchain_images = device.getSwapchainImagesKHR(new_swapchain.get());
    } catch (const vk::OutOfDateKHRError &e) {
        //- no need to recreate, recreation will happen in acquireNextImage
    } catch (const vk::SystemError &e) {
        return e.code();
    }
    m_swapchain = std::move(new_swapchain);
    m_swapchain_images = std::move(swapchain_images);   
    //todo command buffer to transit image layout
    return {};
}

std::error_code Swapchain::acquireNextImage() noexcept
{
    auto expected_target_available_semaphore = this->acquireSemaphore();
    if (not expected_target_available_semaphore) { return expected_target_available_semaphore.error(); }
    vk::UniqueSemaphore target_available = std::move(expected_target_available_semaphore.value());
    vk::Device device = m_device_context_p->getDevice();
    vk::Result result = vk::Result::eErrorUnknown;
    try {
        std::tie(result, m_image_index) = device.acquireNextImageKHR(m_swapchain.get(), std::numeric_limits<uint64_t>::max(), target_available.get(), nullptr);
    } catch (const vk::OutOfDateKHRError &) {
        m_semaphore_pool.emplace(std::move(target_available));
        if (auto ec = this->recreate()) { return ec; }
        return this->acquireNextImage();
    } catch (const vk::SystemError & e) {
        m_semaphore_pool.emplace(std::move(target_available));
        return e.code();
    }
    m_present_resources.m_target_available = std::move(target_available);
    return {};
}

void Swapchain::recyclePresentResources(PresentResources & present_resources) noexcept
{
    vk::Device device = m_device_context_p->getDevice();
    m_present_resources.m_leases.clear();
    if (m_present_resources.m_cmd) {
        present_resources.m_cmd.reset();
        m_cmd_buffer_pool.emplace(std::exchange(present_resources.m_cmd, nullptr));
    }
    if (m_present_resources.m_present_fence) {
        device.resetFences(present_resources.m_present_fence.get());
        m_fence_pool.emplace(std::exchange(present_resources.m_present_fence, {}));
    }
    if (m_present_resources.m_target_available) {
        m_semaphore_pool.emplace(std::exchange(present_resources.m_target_available, {}));
    }
    if (m_present_resources.m_present_ready) {
        m_semaphore_pool.emplace(std::exchange(present_resources.m_present_ready, {}));
    }
}

void Swapchain::tryRecyclePendingResources() noexcept
{
    vk::Device device = m_device_context_p->getDevice();
    while (not m_pending_resources_queue.empty()) {
        PresentResources & present_resources = m_pending_resources_queue.front();
        if (device.getFenceStatus(present_resources.m_present_fence.get()) != vk::Result::eSuccess) { break; }
        this->recyclePresentResources(present_resources);
        m_pending_resources_queue.pop();
    }
}

std::expected<vk::CommandBuffer, std::error_code> Swapchain::acquireCmdBuffer() noexcept
{
    vk::CommandBuffer cmd_buffer;
    if (not m_cmd_buffer_pool.empty()) {
        cmd_buffer = std::move(m_cmd_buffer_pool.front());
        m_cmd_buffer_pool.pop();
        return cmd_buffer;
    }
    vk::Device device = m_device_context_p->getDevice();
    vk::CommandBufferAllocateInfo cmd_buffer_info { m_cmd_pool.get(), vk::CommandBufferLevel::ePrimary, 1u };
    try {
        cmd_buffer = device.allocateCommandBuffers(cmd_buffer_info).front();
    } catch (const vk::SystemError &e) {
        return std::unexpected(e.code());
    }
    return cmd_buffer;
}

std::expected<vk::UniqueFence, std::error_code> Swapchain::acquireFence() noexcept
{
    vk::UniqueFence fence;
    if (not m_fence_pool.empty()) {
        fence = std::move(m_fence_pool.front());
        m_fence_pool.pop();
        return fence;
    }
    vk::Device device = m_device_context_p->getDevice();
    try {
        fence = device.createFenceUnique({});
    } catch (const vk::SystemError &e) {
        return std::unexpected(e.code());
    }
    return fence;
}

std::expected<vk::UniqueSemaphore, std::error_code> Swapchain::acquireSemaphore() noexcept
{
    vk::UniqueSemaphore semaphore;
    if (not m_semaphore_pool.empty()) {
        semaphore = std::move(m_semaphore_pool.front());
        m_semaphore_pool.pop();
        return semaphore;
    }
    vk::Device device = m_device_context_p->getDevice();
    try {
        semaphore = device.createSemaphoreUnique({});
    } catch (const vk::SystemError &e) {
        return std::unexpected(e.code());
    }
    return semaphore;
}

} // namespace lcf::vkc
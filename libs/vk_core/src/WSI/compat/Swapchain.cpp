#include "vk_core/WSI/compat/Swapchain.h"
#include "vk_core/WSI/entry.h"
#include "vk_core/sync/entry.h"
#include "vk_core/manifest/DeviceExtensionManifest.h"
#include "vk_core/WSI/create_surface.h"
#include "vk_core/WSI/WindowHandle.h"
#include "vk_core/error.h"
#include <limits>
#include <algorithm>

namespace stdr = std::ranges;

namespace lcf::vkc::entry {

void register_compat_swapchain(DeviceExtensionManifest & manifest) noexcept
{
    static constexpr std::array k_extensions
    {
        vk::KHRSwapchainExtensionName,
    };

    manifest.addRequiredExtensions(k_extensions);
    register_timeline_semaphore(manifest);
}

} // namespace lcf::vkc::entry

namespace lcf::vkc::wsi::compat {

std::error_code Swapchain::create(
    vk::Instance instance, vk::PhysicalDevice physical_device, vk::Device device,
    uint32_t present_queue_family_index, vk::Queue present_queue,
    const WindowHandle &window_handle) noexcept
{
    m_physical_device = physical_device;
    m_device = device;
    m_present_queue = present_queue;
    auto expected_surface = create_surface(instance, window_handle);
    if (not expected_surface) { return expected_surface.error(); }
    m_surface = std::move(expected_surface.value());
    bool supported = false;   
    try {
        supported = m_physical_device.getSurfaceSupportKHR(present_queue_family_index, m_surface.get());
    } catch (const vk::SystemError & e) { return e.code(); }
    if (not supported) { return errc::no_suitable_present_queue_family; }
    if (auto ec = m_blit_timeline.create(device)) { return ec; }
    vk::CommandPoolCreateInfo pool_info {vk::CommandPoolCreateFlagBits::eResetCommandBuffer, present_queue_family_index };
    try {
        m_cmd_pool = m_device.createCommandPoolUnique(pool_info);
    } catch (vk::SystemError &e) {
        return e.code();
    }
    return {};
}

std::expected<vk::SemaphoreSubmitInfo, std::error_code> Swapchain::_present(
    const std::array<vk::Offset3D, 2> &src_offsets,
    vk::Image src_image,
    ResourceLease image_lease,
    vk::SemaphoreSubmitInfo wait_info,
    vk::ImageSubresourceLayers src_subresource_layers) noexcept
{
    //- 1. check pre-conditions
    //- consider this if condition as a dirty flag, swapchain becomes dirty after first creation or any change in desired params
    if (auto snapshot = m_desired_params_snapshot.loadIfChanged()) {
        if (auto ec = this->recreate(*snapshot)) { return std::unexpected(ec); }
    }
    //- 2. fill m_present_resources
    if (auto ec = this->acquireNextImage()) { return std::unexpected(ec); }
    auto expected_cmd = this->acquireCmdBuffer();
    auto expected_fence = this->acquireFence();
    if (not expected_cmd) { return std::unexpected(expected_cmd.error()); }
    if (not expected_fence) { return std::unexpected(expected_fence.error()); }
    m_present_resources.m_cmd = expected_cmd.value();
    m_present_resources.m_submit_fence = std::move(expected_fence.value());
    if (image_lease) {
        m_present_resources.m_leases.emplace_back(std::move(image_lease));
    }
    vk::CommandBuffer cmd = m_present_resources.m_cmd;
    vk::Semaphore target_available = m_present_resources.m_target_available.get();
    vk::Semaphore present_ready = m_present_ready_semaphores[m_image_index].get();
    vk::Fence submit_fence = m_present_resources.m_submit_fence.get();

    //- 3. transit image layout and blit src_image
    vk::Image dst_image = m_swapchain_images[m_image_index];
    vk::ImageMemoryBarrier to_transfer_dst_barrier, to_present_src_barrier;
    to_transfer_dst_barrier.setImage(dst_image)
        .setOldLayout(vk::ImageLayout::eUndefined)
        .setNewLayout(vk::ImageLayout::eTransferDstOptimal)
        .setSubresourceRange({vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1})
        .setSrcAccessMask(vk::AccessFlagBits::eNone)
        .setDstAccessMask(vk::AccessFlagBits::eTransferWrite);
    to_present_src_barrier.setImage(dst_image)
        .setOldLayout(vk::ImageLayout::eTransferDstOptimal)
        .setNewLayout(vk::ImageLayout::ePresentSrcKHR)
        .setSubresourceRange({vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1})
        .setSrcAccessMask(vk::AccessFlagBits::eTransferWrite)
        .setDstAccessMask(vk::AccessFlagBits::eNone);

    vk::ImageBlit blit_region = {};
    blit_region.setSrcSubresource(src_subresource_layers)
        .setDstSubresource({vk::ImageAspectFlagBits::eColor, 0, 0, 1})
        .setSrcOffsets(src_offsets)
        .setDstOffsets({vk::Offset3D {0, 0, 0}, {static_cast<int32_t>(m_width), static_cast<int32_t>(m_height), 1}});

    cmd.begin(vk::CommandBufferBeginInfo(vk::CommandBufferUsageFlagBits::eOneTimeSubmit));
    cmd.pipelineBarrier(
        vk::PipelineStageFlagBits::eTopOfPipe,
        vk::PipelineStageFlagBits::eTransfer,
        {}, {}, {}, to_transfer_dst_barrier);
    cmd.blitImage(
        src_image, vk::ImageLayout::eTransferSrcOptimal,
        dst_image, vk::ImageLayout::eTransferDstOptimal,
        blit_region, vk::Filter::eLinear);
    cmd.pipelineBarrier(
        vk::PipelineStageFlagBits::eTransfer,
        vk::PipelineStageFlagBits::eBottomOfPipe,
        {}, {}, {}, to_present_src_barrier);
    cmd.end();

    //- 4. submit cmd
    std::array<vk::Semaphore, 2> wait_semaphores { target_available, wait_info.semaphore };
    std::array<vk::PipelineStageFlags, 2> wait_stages { vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eTransfer };
    uint32_t wait_count = wait_info.semaphore ? 2u : 1u;
    vk::SemaphoreSubmitInfo blit_timeline_signal = m_blit_timeline.advanceTarget().generateSubmitInfo();
    vk::Semaphore signal_semaphores[] { present_ready, blit_timeline_signal.semaphore };
    uint64_t signal_values[] { 0u, blit_timeline_signal.value };
    vk::TimelineSemaphoreSubmitInfo timeline_info;
    timeline_info.setSignalSemaphoreValues(signal_values);
    vk::SubmitInfo submit_info;
    submit_info.setWaitSemaphoreCount(wait_count)
        .setPWaitSemaphores(wait_semaphores.data())
        .setPWaitDstStageMask(wait_stages.data())
        .setCommandBuffers(cmd)
        .setSignalSemaphores(signal_semaphores)
        .setPNext(&timeline_info);
    try {
        m_present_queue.submit(submit_info, submit_fence);
    } catch (const vk::SystemError & e) {
        this->recyclePresentResources(m_present_resources);
        return std::unexpected(e.code());
    }
    //- 5. present
    vk::PresentInfoKHR present_info;
    present_info.setWaitSemaphores(present_ready)
        .setSwapchains(m_swapchain.get())
        .setPImageIndices(&m_image_index);
    vk::Result present_result = vk::Result::eSuccess;
    try {
        present_result = m_present_queue.presentKHR(present_info);
    } catch (const vk::OutOfDateKHRError &e) {
        //- no need to recreate, recreation will happen in acquireNextImage, keep present_result as vk::Result::eSuccess
    } catch (const vk::SystemError &e) {
        present_result = static_cast<vk::Result>(e.code().value());
    }
    //- 6. recycle resources, except present_ready semaphore
    m_pending_resources_queue.emplace(std::exchange(m_present_resources, {}));
    this->tryRecyclePendingResources();
    if (present_result == vk::Result::eSuccess or present_result == vk::Result::eSuboptimalKHR) {
        blit_timeline_signal.setStageMask(vk::PipelineStageFlagBits2::eColorAttachmentOutput);
        return blit_timeline_signal;
    }
    return std::unexpected(vk::make_error_code(present_result));
}

std::expected<vk::SemaphoreSubmitInfo, std::error_code> Swapchain::present(
    const std::array<vk::Offset3D, 2> &src_offsets,
    vk::Image src_image,
    ResourceLease image_lease,
    vk::SemaphoreSubmitInfo wait_info,
    vk::ImageSubresourceLayers src_subresource_layers) noexcept
{
    m_cached_present_input.write({src_offsets, src_image, image_lease, wait_info, src_subresource_layers});
    if (m_resize_has_priority.load(std::memory_order_acquire)) { return std::unexpected(errc::present_skipped_for_resize); }
    std::lock_guard lock(m_present_mutex);
    return this->_present(src_offsets, src_image, std::move(image_lease), wait_info, src_subresource_layers);
}

std::error_code Swapchain::resizeToFit() noexcept
{
    struct AtomicSwitchFlagGuard
    {
        ~AtomicSwitchFlagGuard() { m_switch_flag.store(false, std::memory_order_release); }
        AtomicSwitchFlagGuard(std::atomic<bool> & switch_flag) : m_switch_flag(switch_flag) { m_switch_flag.store(true, std::memory_order_release); }
        std::atomic<bool> & m_switch_flag;
    };
    AtomicSwitchFlagGuard guard {m_resize_has_priority};
    std::lock_guard lock(m_present_mutex);
    auto [src_offsets, src_image, image_lease, wait_info, src_subresource_layers] = m_cached_present_input.read();
    if (not src_image) { return {}; }
    auto expected_present_result = this->_present(src_offsets, src_image, image_lease, wait_info, src_subresource_layers);
    if (not expected_present_result) { return expected_present_result.error(); }
    return {};
}

std::error_code Swapchain::recreate(const DesiredParams & desired_params) noexcept
{
    vk::SurfaceCapabilitiesKHR surface_capabilities;
    try {
        surface_capabilities = m_physical_device.getSurfaceCapabilitiesKHR(m_surface.get());
    } catch (vk::SystemError &e) {
        return e.code();
    }
    auto [cur_width, cur_height] = surface_capabilities.currentExtent;
    m_width = std::clamp(cur_width, surface_capabilities.minImageExtent.width, surface_capabilities.maxImageExtent.width);
    m_height = std::clamp(cur_height, surface_capabilities.minImageExtent.height, surface_capabilities.maxImageExtent.height);
    if (m_width == 0 or m_height == 0) { return errc::surface_zero_size; };
    //- validate desired params against device support, fall back if unsupported
    std::vector<vk::SurfaceFormatKHR> surface_formats;
    std::vector<vk::PresentModeKHR> present_modes;
    try {
        surface_formats = m_physical_device.getSurfaceFormatsKHR(m_surface.get());
        present_modes = m_physical_device.getSurfacePresentModesKHR(m_surface.get());
    } catch (vk::SystemError &e) {
        return e.code();
    }
    auto surface_it = stdr::find(surface_formats, desired_params.surface_format);
    auto present_it = stdr::find(present_modes, desired_params.present_mode);
    m_surface_format = surface_it != surface_formats.end() ? *surface_it : surface_formats.front();
    m_present_mode = present_it != present_modes.end() ? *present_it : vk::PresentModeKHR::eFifo;  

    vk::CompositeAlphaFlagBitsKHR composite_bit = vk::CompositeAlphaFlagBitsKHR::eOpaque;
    while (composite_bit & vk::FlagTraits<vk::CompositeAlphaFlagBitsKHR>::allFlags) {
        using MaskType = typename vk::CompositeAlphaFlagsKHR::MaskType;
        if (surface_capabilities.supportedCompositeAlpha & composite_bit) { break; }
        composite_bit = vk::CompositeAlphaFlagBitsKHR(MaskType(composite_bit) << 1);
    }

    vk::UniqueSwapchainKHR new_swapchain;
    std::vector<vk::Image> swapchain_images;
    uint32_t image_count = std::clamp(desired_params.image_count,
        surface_capabilities.minImageCount,
        std::max(surface_capabilities.maxImageCount, desired_params.image_count));
    
    vk::SwapchainCreateInfoKHR swapchain_info;
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
        new_swapchain = m_device.createSwapchainKHRUnique(swapchain_info);
        swapchain_images = m_device.getSwapchainImagesKHR(new_swapchain.get());
    } catch (const vk::OutOfDateKHRError &e) {
        //- no need to recreate, recreation will happen in acquireNextImage
        return vk::Result::eSuccess;
    } catch (const vk::SystemError &e) {
        return e.code();
    }

    m_device.waitIdle();
    m_swapchain = std::move(new_swapchain);
    m_swapchain_images = std::move(swapchain_images);
    while (not m_pending_resources_queue.empty()) {
        this->recyclePresentResources(m_pending_resources_queue.front());
        m_pending_resources_queue.pop();
    }
    m_present_ready_semaphores.resize(m_swapchain_images.size());
    try {
        for (auto & semaphore : m_present_ready_semaphores) { semaphore = m_device.createSemaphoreUnique({}); }
    } catch (const vk::SystemError & e) {
        return e.code();
    }
    return {};
}

std::error_code Swapchain::acquireNextImage() noexcept
{
    auto expected_semaphore = this->acquireSemaphore();
    if (not expected_semaphore) { return expected_semaphore.error(); }
    vk::UniqueSemaphore target_available = std::move(expected_semaphore.value());
    vk::Result result = vk::Result::eErrorUnknown;
    try {
        std::tie(result, m_image_index) = m_device.acquireNextImageKHR(
            m_swapchain.get(),
            std::numeric_limits<uint64_t>::max(),
            target_available.get(), nullptr);
    } catch (const vk::OutOfDateKHRError &) {
        m_semaphore_pool.emplace(std::move(target_available));
        //- m_consumed is guaranteed non-null here (set by _present()'s precondition block)
        if (auto ec = this->recreate(m_desired_params_snapshot.read().value())) { return ec; }
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
    present_resources.m_leases.clear();
    if (present_resources.m_cmd) {
        present_resources.m_cmd.reset();
        m_cmd_buffer_pool.emplace(std::exchange(present_resources.m_cmd, nullptr));
    }
    if (present_resources.m_submit_fence) {
        m_device.resetFences(present_resources.m_submit_fence.get());
        m_fence_pool.emplace(std::exchange(present_resources.m_submit_fence, {}));
    }
    if (present_resources.m_target_available) {
        m_semaphore_pool.emplace(std::exchange(present_resources.m_target_available, {}));
    }
}

void Swapchain::tryRecyclePendingResources() noexcept
{
    while (not m_pending_resources_queue.empty()) {
        PresentResources & front = m_pending_resources_queue.front();
        if (m_device.getFenceStatus(front.m_submit_fence.get()) != vk::Result::eSuccess) { break; }
        this->recyclePresentResources(front);
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
    vk::CommandBufferAllocateInfo cmd_buffer_info { m_cmd_pool.get(), vk::CommandBufferLevel::ePrimary, 1u };
    try {
        cmd_buffer = m_device.allocateCommandBuffers(cmd_buffer_info).front();
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
    try {
        fence = m_device.createFenceUnique({});
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
    try {
        semaphore = m_device.createSemaphoreUnique({});
    } catch (const vk::SystemError &e) {
        return std::unexpected(e.code());
    }
    return semaphore;
}

auto Swapchain::setDesiredSwapchainImageCount(uint32_t desired_count) noexcept -> Self &
{
    m_desired_params_snapshot.template update<&DesiredParams::image_count>(desired_count);
    return *this;
}

auto Swapchain::setDesiredSurfaceFormat(const vk::SurfaceFormatKHR &surface_format) noexcept -> Self &
{
    m_desired_params_snapshot.template update<&DesiredParams::surface_format>(surface_format);
    return *this;
}

auto Swapchain::setDesiredPresentMode(const vk::PresentModeKHR &present_mode) noexcept -> Self &
{
    m_desired_params_snapshot.template update<&DesiredParams::present_mode>(present_mode);
    return *this;
}

} // namespace lcf::vkc::wsi::compat
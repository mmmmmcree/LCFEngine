#include "vk_core/WSI/Swapchain.h"
#include "vk_core/WSI/entry.h"
#include "vk_core/manifest/DeviceExtensionManifest.h"
#include "vk_core/WSI/create_surface.h"
#include "vk_core/WSI/WindowHandle.h"
#include "vk_core/context/RenderDeviceContext.h"
#include "vk_core/context/QueueContext.h"
#include "vk_core/error.h"
#include <limits>
#include <algorithm>

namespace stdr = std::ranges;

namespace lcf::vkc::wsi {

void register_swapchain(DeviceExtensionManifest & manifest) noexcept
{
    static constexpr std::array k_extensions
    {
        vk::KHRSwapchainExtensionName,
        vk::KHRSwapchainMaintenance1ExtensionName
    };
    static constexpr std::array k_features
    {
        utils::t_feature_bit<&vk::PhysicalDeviceSwapchainMaintenance1FeaturesEXT::swapchainMaintenance1>,
    };
    // if constexpr xxx
    manifest.addRequiredExtensions(k_extensions)
        .addRequiredFeatures(k_features);
}

std::error_code Swapchain::create(
    vk::Instance instance,
    RenderDeviceContext &device_context,
    const WindowHandle &window_handle) noexcept
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
    vk::CommandPoolCreateInfo pool_info {vk::CommandPoolCreateFlagBits::eResetCommandBuffer, present_family_index };
    vk::Device device = device_context.getDevice();
    try {
        m_cmd_pool = device.createCommandPoolUnique(pool_info);
    } catch (vk::SystemError &e) {
        return e.code();
    }
    m_consumed_desired_params_sp = {};
    return {};
}

std::error_code Swapchain::present(
    vk::SemaphoreSubmitInfo wait_info,
    vk::Image src_image,
    ResourceLease image_lease,
    const std::array<vk::Offset3D, 2> &src_offsets,
    vk::ImageSubresourceLayers src_subresource_layers) noexcept
{
    //- 1. check pre-conditions
    //- consider this if condition as a dirty flag, swapchain becomes dirty after first creation or any change in desired params
    auto desired_params_sp = m_provided_desired_params_asp.load(std::memory_order_acquire);
    if (desired_params_sp != m_consumed_desired_params_sp) { 
        if (auto ec = this->recreate(*desired_params_sp)) { return ec; }
        m_consumed_desired_params_sp = std::move(desired_params_sp);
    }
    if (auto ec = this->acquireNextImage()) { return ec; }
    //- 2. fill m_present_resources
    auto expected_cmd = this->acquireCmdBuffer();
    auto expected_fence = this->acquireFence();
    auto expected_present_ready_semaphore = this->acquireSemaphore();
    if (not expected_cmd) { return expected_cmd.error(); }
    if (not expected_fence) { return expected_fence.error(); }
    if (not expected_present_ready_semaphore) { return expected_present_ready_semaphore.error(); }
    auto & cmd = m_present_resources.m_cmd = std::move(expected_cmd.value());
    auto & present_fence = m_present_resources.m_present_fence = std::move(expected_fence.value());
    auto & present_ready = m_present_resources.m_present_ready = std::move(expected_present_ready_semaphore.value());
    auto & target_available = m_present_resources.m_target_available;
    if (image_lease) {
        m_present_resources.m_leases.emplace_back(std::move(image_lease));
    }

    //- 3. transit image layout and blit src_image
    vk::Image dst_image = m_swapchain_images[m_image_index];
    vk::ImageMemoryBarrier2 to_transfer_dst_barrier, to_present_src_barrier;
    to_transfer_dst_barrier.setImage(dst_image)
        .setOldLayout(vk::ImageLayout::eUndefined)
        .setNewLayout(vk::ImageLayout::eTransferDstOptimal)
        .setSubresourceRange({vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1})
        .setSrcStageMask(vk::PipelineStageFlagBits2::eTopOfPipe)
        .setSrcAccessMask(vk::AccessFlagBits2::eNone)
        .setDstStageMask(vk::PipelineStageFlagBits2::eBlit)
        .setDstAccessMask(vk::AccessFlagBits2::eTransferWrite);
    to_present_src_barrier.setImage(dst_image)
        .setOldLayout(vk::ImageLayout::eTransferDstOptimal)
        .setNewLayout(vk::ImageLayout::ePresentSrcKHR)
        .setSubresourceRange({vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1})
        .setSrcStageMask(vk::PipelineStageFlagBits2::eBlit)
        .setSrcAccessMask(vk::AccessFlagBits2::eTransferWrite)
        .setDstStageMask(vk::PipelineStageFlagBits2::eBottomOfPipe)
        .setDstAccessMask(vk::AccessFlagBits2::eNone);

    vk::DependencyInfo to_transfer_dst_barrier_dep_info, to_present_src_barrier_dep_info;
    to_transfer_dst_barrier_dep_info.setImageMemoryBarriers(to_transfer_dst_barrier);
    to_present_src_barrier_dep_info.setImageMemoryBarriers(to_present_src_barrier);
    vk::ImageBlit2 blit_region = {};
    blit_region.setSrcSubresource(src_subresource_layers)
        .setDstSubresource({vk::ImageAspectFlagBits::eColor, 0, 0, 1})
        .setSrcOffsets(src_offsets)
        .setDstOffsets({vk::Offset3D {0, 0, 0}, {static_cast<int32_t>(m_width), static_cast<int32_t>(m_height), 1}});
    vk::BlitImageInfo2 blit_info = {};
    blit_info.setSrcImage(src_image)
        .setSrcImageLayout(vk::ImageLayout::eTransferSrcOptimal)
        .setDstImage(dst_image)
        .setDstImageLayout(vk::ImageLayout::eTransferDstOptimal)
        .setRegions(blit_region)
        .setFilter(vk::Filter::eLinear);

    cmd.begin(vk::CommandBufferBeginInfo(vk::CommandBufferUsageFlagBits::eOneTimeSubmit));
    cmd.pipelineBarrier2(to_transfer_dst_barrier_dep_info);
    cmd.blitImage2(blit_info);
    cmd.pipelineBarrier2(to_present_src_barrier_dep_info);
    cmd.end();

    //- 4. submit cmd
    vk::PipelineStageFlags wait_stage = vk::PipelineStageFlagBits::eTransfer;
    vk::SubmitInfo submit_info;
    submit_info.setWaitSemaphores(target_available.get())
        .setWaitDstStageMask(wait_stage)
        .setCommandBuffers(cmd)
        .setSignalSemaphores(present_ready.get());
    vk::Queue present_queue = m_device_context_p->getGraphicsQueueContext().getQueue();
    try {
        present_queue.submit(submit_info);
    } catch (const vk::SystemError & e) {
        this->recyclePresentResources(m_present_resources);
        return e.code();
    }
    //- 5. present
    auto device = m_device_context_p->getDevice();
    vk::StructureChain<vk::PresentInfoKHR, vk::SwapchainPresentFenceInfoKHR> present_info_chain;
    present_info_chain.get<vk::SwapchainPresentFenceInfoKHR>().setFences(present_fence.get());
    present_info_chain.get<vk::PresentInfoKHR>()
        .setWaitSemaphores(present_ready.get())
        .setSwapchains(m_swapchain.get())
        .setPImageIndices(&m_image_index);
    vk::Result present_result = vk::Result::eSuccess;   
    try {
        present_result = present_queue.presentKHR(present_info_chain.get<vk::PresentInfoKHR>());
    } catch (const vk::OutOfDateKHRError &e) {
        //- no need to recreate, recreation will happen in acquireNextImage, keep present_result as vk::Result::eSuccess
    } catch (const vk::SystemError &e) {
        present_result = static_cast<vk::Result>(e.code().value());
    }
    //- 6. recycle resources
    m_pending_resources_queue.emplace(std::exchange(m_present_resources, {}));
    this->tryRecyclePendingResources();
    if (present_result == vk::Result::eSuccess or present_result == vk::Result::eSuboptimalKHR) { return {}; }
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

std::error_code Swapchain::recreate(const DesiredParams & desired_params) noexcept
{
    vk::PhysicalDevice physical_device = m_device_context_p->getPhysicalDevice();
    vk::SurfaceCapabilitiesKHR surface_capabilities;
    try {
        surface_capabilities = physical_device.getSurfaceCapabilitiesKHR(m_surface.get());
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
        surface_formats = physical_device.getSurfaceFormatsKHR(m_surface.get());
        present_modes = physical_device.getSurfacePresentModesKHR(m_surface.get());
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

    vk::Device device = m_device_context_p->getDevice();
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
        new_swapchain = device.createSwapchainKHRUnique(swapchain_info);
        swapchain_images = device.getSwapchainImagesKHR(new_swapchain.get());
    } catch (const vk::OutOfDateKHRError &e) {
        //- no need to recreate, recreation will happen in acquireNextImage
        return vk::Result::eSuccess;
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
        std::tie(result, m_image_index) = device.acquireNextImageKHR(
            m_swapchain.get(),
            std::numeric_limits<uint64_t>::max(),
            target_available.get(), nullptr);
    } catch (const vk::OutOfDateKHRError &) {
        m_semaphore_pool.emplace(std::move(target_available));
        //- m_consumed_desired_params_sp is guaranteed non-null here (set by present()'s precondition block)
        if (auto ec = this->recreate(*m_consumed_desired_params_sp)) { return ec; }
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
    present_resources.m_leases.clear();
    if (present_resources.m_cmd) {
        present_resources.m_cmd.reset();
        m_cmd_buffer_pool.emplace(std::exchange(present_resources.m_cmd, nullptr));
    }
    if (present_resources.m_present_fence) {
        device.resetFences(present_resources.m_present_fence.get());
        m_fence_pool.emplace(std::exchange(present_resources.m_present_fence, {}));
    }
    if (present_resources.m_target_available) {
        m_semaphore_pool.emplace(std::exchange(present_resources.m_target_available, {}));
    }
    if (present_resources.m_present_ready) {
        m_semaphore_pool.emplace(std::exchange(present_resources.m_present_ready, {}));
    }
}

void Swapchain::tryRecyclePendingResources() noexcept
{
    vk::Device device = m_device_context_p->getDevice();
    while (not m_pending_resources_queue.empty()) {
        PresentResources & present_resources = m_pending_resources_queue.front();
        vk::Result present_fence_status = device.getFenceStatus(present_resources.m_present_fence.get());
        if (present_fence_status != vk::Result::eSuccess) { break; }
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

template <typename Mutator>
auto Swapchain::updateDesired(Mutator && mutator) noexcept -> Self &
{
    auto current = m_provided_desired_params_asp.load(std::memory_order_acquire);
    while (true) {
        auto next = std::make_shared<DesiredParams>(*current);
        mutator(*next);
        if (*next == *current) { return *this; }
        if (m_provided_desired_params_asp.compare_exchange_weak(
            current, std::move(next),
            std::memory_order_release, std::memory_order_acquire)) { return *this; }
    }
    return *this;
}

auto Swapchain::setDesiredSwapchainImageCount(uint32_t desired_count) noexcept -> Self &
{
    return this->updateDesired([&](DesiredParams & p) { p.image_count = desired_count; });
}

auto Swapchain::setDesiredSurfaceFormat(const vk::SurfaceFormatKHR &surface_format) noexcept -> Self &
{
    return this->updateDesired([&](DesiredParams & p) { p.surface_format = surface_format; });
}

auto Swapchain::setDesiredPresentMode(const vk::PresentModeKHR &present_mode) noexcept -> Self &
{
    return this->updateDesired([&](DesiredParams & p) { p.present_mode = present_mode; });
}

} // namespace lcf::vkc
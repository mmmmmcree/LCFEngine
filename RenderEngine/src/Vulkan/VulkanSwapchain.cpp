#include "VulkanSwapchain.h"
#include "VulkanContext.h"
#include "error.h"
#include "enum_name.h"

using namespace lcf::render;

VulkanSwapchain::VulkanSwapchain(UniqueSurface && unique_surface) :
    RenderTarget(),
    m_surface(std::move(unique_surface))
{
}

lcf::render::VulkanSwapchain::~VulkanSwapchain()
{
    auto device = m_context_p->getDevice();
    device.waitIdle();
}

void VulkanSwapchain::create(VulkanContext * context_p)
{
    if (this->isCreated()) { return; }
    m_context_p = context_p;
    vk::PhysicalDevice physical_device = m_context_p->getPhysicalDevice();
    auto surface_formats = physical_device.getSurfaceFormatsKHR(this->getSurface());
    auto present_modes = physical_device.getSurfacePresentModesKHR(this->getSurface());
    m_surface_capabilities = physical_device.getSurfaceCapabilitiesKHR(this->getSurface());
    auto surface_it = std::ranges::find_if(surface_formats, [](const auto& f) {
        return f == vk::SurfaceFormatKHR{vk::Format::eB8G8R8A8Srgb, vk::ColorSpaceKHR::eSrgbNonlinear};
    });
    auto present_it = std::ranges::find_if(present_modes, [](const auto& m) {
        return m == vk::PresentModeKHR::eMailbox;
    });
    m_surface_format = surface_it != surface_formats.end() ? *surface_it : surface_formats.front();
    m_present_mode = present_it != present_modes.end() ? *present_it : vk::PresentModeKHR::eFifo;   
}

bool VulkanSwapchain::prepareForRender()
{
    if (m_silent) { return false; }
    if (not m_swapchain) { this->recreate(); }
    return this->acquireAvailableTarget();
}

bool VulkanSwapchain::finishRender()
{
    bool successful = this->present();
    if (successful) {
        this->tryRecycle();
    }
    return successful;
}

bool VulkanSwapchain::recreate()
{
    vk::PhysicalDevice physical_device = m_context_p->getPhysicalDevice();
    m_surface_capabilities = physical_device.getSurfaceCapabilitiesKHR(this->getSurface());
    auto [cur_width, cur_height] = m_surface_capabilities.currentExtent;
    this->setExtent(cur_width, cur_height);
    if (cur_width == 0 or cur_height == 0) { return false; }
    
    vk::CompositeAlphaFlagBitsKHR composite_bit = vk::CompositeAlphaFlagBitsKHR::eOpaque;
    while (composite_bit & vk::FlagTraits<vk::CompositeAlphaFlagBitsKHR>::allFlags) {
        using MaskType = typename vk::CompositeAlphaFlagsKHR::MaskType;
        if (m_surface_capabilities.supportedCompositeAlpha & composite_bit) { break; }
        composite_bit = vk::CompositeAlphaFlagBitsKHR(MaskType(composite_bit) << 1);
    }

    vk::Device device = m_context_p->getDevice();
    vk::UniqueSwapchainKHR new_swapchain;
    std::vector<vk::Image> swapchain_images;
    vk::SwapchainCreateInfoKHR swapchain_info;
    swapchain_info.setSurface(this->getSurface())
        .setMinImageCount(m_swapchain_images.size())
        .setImageFormat(m_surface_format.format)
        .setImageColorSpace(m_surface_format.colorSpace)
        .setImageExtent(this->getExtent())
        .setImageArrayLayers(1)
        .setImageUsage(vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eTransferDst)
        .setImageSharingMode(vk::SharingMode::eExclusive)
        .setPreTransform(m_surface_capabilities.currentTransform)
        .setCompositeAlpha(composite_bit)
        .setPresentMode(m_present_mode)
        .setClipped(true)
        .setOldSwapchain(m_swapchain.get());
    try {
        new_swapchain = device.createSwapchainKHRUnique(swapchain_info);
        swapchain_images = device.getSwapchainImagesKHR(new_swapchain.get());
    } catch (const vk::OutOfDateKHRError &e) {
        return this->recreate();
    } catch (const vk::SystemError &e) {
        return false;
    }
    m_current_pending_resources.collect(std::move(m_swapchain));
    m_swapchain = std::move(new_swapchain);
    for (int i = 0; i < m_swapchain_images.size(); ++i) {
        auto & swapchain_image = m_swapchain_images[i];
        m_current_pending_resources.collect(std::move(swapchain_image));
        swapchain_image = VulkanImage::makeShared();
        m_swapchain_images[i]->setExtent(vk::Extent3D{ this->getWidth(), this->getHeight(), 1 })
            .setArrayLayers(swapchain_info.imageArrayLayers)
            .setUsage(swapchain_info.imageUsage)
            .setFormat(swapchain_info.imageFormat)
            .create(m_context_p, swapchain_images[i]);
    }
    return true;
}

bool VulkanSwapchain::acquireAvailableTarget()
{
    auto device = m_context_p->getDevice();
    m_current_frame_resources.emplace();
    m_current_frame_resources->setTargetAvailableSemaphore(this->acquireSemaphore());
    vk::Result acquire_result;
    try {
        std::tie(acquire_result, m_image_index) = device.acquireNextImageKHR(m_swapchain.get(), UINT64_MAX, m_current_frame_resources->getTargetAvailableSemaphore(), nullptr);
    } catch (const vk::OutOfDateKHRError & e) {
        this->collectCurrentFrameResources();
        return this->recreate() and this->acquireAvailableTarget();
    } catch (const vk::SystemError &e) {
        this->collectCurrentFrameResources();
        return false;
    }
    m_current_frame_resources->setPresentReadySemaphore(this->acquireSemaphore());
    m_current_frame_resources->setImage(m_swapchain_images[m_image_index]);
    bool successful =  acquire_result == vk::Result::eSuccess or acquire_result == vk::Result::eSuboptimalKHR;
    if (not successful) { this->collectCurrentFrameResources(); }
    return successful;
}

bool VulkanSwapchain::present()
{
    auto device = m_context_p->getDevice();
    auto unique_present_fence = this->acquireFence();
    vk::StructureChain<vk::PresentInfoKHR, vk::SwapchainPresentFenceInfoEXT> present_info;
    vk::Fence present_fence = unique_present_fence.get();
    vk::Semaphore present_ready_semaphore = m_current_frame_resources->getPresentReadySemaphore();
    present_info.get<vk::SwapchainPresentFenceInfoEXT>().setFences(present_fence);
    present_info.get<vk::PresentInfoKHR>().setWaitSemaphores(present_ready_semaphore)
        .setSwapchains(m_swapchain.get())
        .setPImageIndices(&m_image_index);
    m_current_pending_resources.setPresentFence(std::move(unique_present_fence));
    this->collectCurrentFrameResources();
    this->collectCurrentPendingResources();
    vk::Result present_result = vk::Result::eErrorUnknown;
    auto present_queue = m_context_p->getQueue(vk::QueueFlagBits::eGraphics);
    try {
        present_result = present_queue.presentKHR(present_info.get<vk::PresentInfoKHR>());
    } catch (const vk::OutOfDateKHRError &e) {
        //- no need to recreate, recreation will happen in acquireAvailableTarget
    } catch (const vk::SystemError &e) {
        return false;
    }
    bool successful = present_result == vk::Result::eSuccess or present_result == vk::Result::eSuboptimalKHR;
    return successful;
}

void lcf::render::VulkanSwapchain::collectCurrentFrameResources()
{
    if (not m_current_frame_resources) { return; }
    m_current_pending_resources.collect(std::move(*m_current_frame_resources));
    m_current_frame_resources = std::nullopt;
}

void lcf::render::VulkanSwapchain::collectCurrentPendingResources()
{
    auto & resources = m_current_pending_resources;
    if (not resources.m_present_fence) { return; }
    m_recycle_queue.emplace(std::move(resources));
    resources = {};
}

void VulkanSwapchain::tryRecycle()
{
    auto device = m_context_p->getDevice();
    while (not m_recycle_queue.empty()) {
        auto & resources = m_recycle_queue.front();
        auto & fence = resources.m_present_fence;
        vk::Result status = device.getFenceStatus(fence.get());
        if (status != vk::Result::eSuccess) { break; }
        device.resetFences(fence.get());
        m_fence_pool.emplace(std::move(fence));
        for (auto & semaphore : resources.m_semaphores) {
            m_semaphore_pool.emplace(std::move(semaphore));
        }
        m_recycle_queue.pop();
    }
}

vk::UniqueFence VulkanSwapchain::acquireFence()
{
    if (m_fence_pool.empty()) {
        return m_context_p->getDevice().createFenceUnique({});
    }
    auto fence = std::move(m_fence_pool.front());
    m_fence_pool.pop();
    return fence;
}

vk::UniqueSemaphore VulkanSwapchain::acquireSemaphore()
{
    if (m_semaphore_pool.empty()) {
        return m_context_p->getDevice().createSemaphoreUnique({});
    }
    auto semaphore = std::move(m_semaphore_pool.front());
    m_semaphore_pool.pop();
    return semaphore;
}

/*
一帧中swapchain执行prepare操作和finish操作。
prepare操作负责：
    1.提供本帧渲染需要的数据包，包括：image index(索引image), target_available_semaphore(acquire操作成功激发), present_ready_semaphore(present操作等待);信号量从成员变量semaphore_pool中获取。
    2.获取image失败的交换链重建，oldswapchain伴随本帧的target_available_semaphore，present_ready_semaphore，以及所有的image进入垃圾堆，同时提供新的target_available_semaphore和present_ready_semaphore。
    3.获取image成功，继续本帧渲染，否则跳过本帧渲染。也就是后续的finish也不会执行。
finish操作负责：
    1.present，附带fence标记present是否完成。
    2.present失败交换链重建。present失败的前提是acquire成功，那么同样也是oldswapchain伴随本帧的target_available_semaphore，present_ready_semaphore，以及所有的image进入垃圾堆。
    3.给当前累积的垃圾堆加入fence，并加入带回收队列；
    4.从头到尾检查队列fence完成情况，回收fence 和semaphores进池，析构image相关内容。
*/

VulkanSwapchain::FrameResources::FrameResources(FrameResources &&other) noexcept :
    m_image_sp(std::move(other.m_image_sp)),
    m_target_available(std::move(other.m_target_available)),
    m_present_ready(std::move(other.m_present_ready))
{
}

VulkanSwapchain::FrameResources & VulkanSwapchain::FrameResources::operator=(FrameResources &&other) noexcept
{
    m_image_sp = std::move(other.m_image_sp);
    m_target_available = std::move(other.m_target_available);
    m_present_ready = std::move(other.m_present_ready);
    return *this;
}

VulkanSwapchain::PendingRecycleResources::PendingRecycleResources(PendingRecycleResources &&other) noexcept :
    m_present_fence(std::move(other.m_present_fence)),
    m_semaphores(std::move(other.m_semaphores)),
    m_images(std::move(other.m_images)),
    m_swapchains(std::move(other.m_swapchains))
{
}

VulkanSwapchain::PendingRecycleResources & VulkanSwapchain::PendingRecycleResources::operator=(PendingRecycleResources &&other) noexcept
{
    m_present_fence = std::move(other.m_present_fence);
    m_semaphores = std::move(other.m_semaphores);
    m_images = std::move(other.m_images);
    m_swapchains = std::move(other.m_swapchains);
    return *this;
}

void lcf::render::VulkanSwapchain::PendingRecycleResources::collect(FrameResources &&resources) noexcept
{
    if (resources.m_image_sp) {
        m_images.emplace_back(std::move(resources.m_image_sp));
    }
    if (resources.m_target_available) {
        m_semaphores.emplace_back(std::move(resources.m_target_available));
    }
    if (resources.m_present_ready) {
        m_semaphores.emplace_back(std::move(resources.m_present_ready));
    }
}

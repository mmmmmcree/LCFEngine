#include "VulkanSwapchain.h"
#include "VulkanContext.h"
#include "error.h"
#include "enum_name.h"

lcf::render::VulkanSwapchain::VulkanSwapchain(VulkanContext *context, vk::SurfaceKHR surface) :
    RenderTarget(),
    m_context_p(context)
{
    vk::Instance instance = context->getInstance();
    m_surface = surface;
}

lcf::render::VulkanSwapchain::~VulkanSwapchain()
{
    m_swapchain.reset();
    auto instance = m_context_p->getInstance();
    instance.destroySurfaceKHR(m_surface);
    m_surface = nullptr;
}

void lcf::render::VulkanSwapchain::create()
{
    if (this->isCreated()) { return; }
    vk::PhysicalDevice physical_device = m_context_p->getPhysicalDevice();
    auto surface_formats = physical_device.getSurfaceFormatsKHR(m_surface);
    auto present_modes = physical_device.getSurfacePresentModesKHR(m_surface);
    vk::SurfaceCapabilitiesKHR surface_capabilities = physical_device.getSurfaceCapabilitiesKHR(m_surface);
    auto surface_it = std::ranges::find_if(surface_formats, [](const auto& f) {
        return f == vk::SurfaceFormatKHR{vk::Format::eB8G8R8A8Srgb, vk::ColorSpaceKHR::eSrgbNonlinear};
    });
    auto present_it = std::ranges::find_if(present_modes, [](const auto& m) {
        return m == vk::PresentModeKHR::eMailbox;
    });
    m_surface_format = surface_it != surface_formats.end() ? *surface_it : surface_formats.front();
    m_present_mode = present_it != present_modes.end() ? *present_it : vk::PresentModeKHR::eFifo;   
    auto device = m_context_p->getDevice();
    vk::SemaphoreCreateInfo semaphore_info;
    for (auto &swapchain_resource : m_swapchain_resources_list) {
        swapchain_resource.target_available = device.createSemaphoreUnique(semaphore_info);
    }
}

bool lcf::render::VulkanSwapchain::recreate()
{
    vk::Device device = m_context_p->getDevice();
    vk::SwapchainCreateInfoKHR swapchain_info;
    swapchain_info.setSurface(m_surface)
        .setMinImageCount(m_swapchain_resources_list.size())
        .setImageFormat(m_surface_format.format)
        .setImageColorSpace(m_surface_format.colorSpace)
        .setImageExtent(this->getExtent())
        .setImageArrayLayers(1)
        .setImageUsage(vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eTransferDst)
        .setPreTransform(vk::SurfaceTransformFlagBitsKHR::eIdentity)
        .setCompositeAlpha(vk::CompositeAlphaFlagBitsKHR::eOpaque)
        .setPresentMode(m_present_mode)
        .setClipped(true)
        .setOldSwapchain(m_swapchain.get());
    uint32_t graphics_queue_family_index = m_context_p->getQueueFamilyIndex(vk::QueueFlagBits::eGraphics);
    uint32_t present_queue_family_index = graphics_queue_family_index;
    swapchain_info.setImageSharingMode(vk::SharingMode::eExclusive)
        .setQueueFamilyIndices(nullptr);
    std::vector<vk::Image> swapchain_images;
    try {
        m_swapchain = device.createSwapchainKHRUnique(swapchain_info);
        swapchain_images = device.getSwapchainImagesKHR(m_swapchain.get());
    } catch (const vk::SystemError &e) {
        return false;
    }
    
    vk::ImageViewCreateInfo image_view_info;
    image_view_info.setViewType(vk::ImageViewType::e2D)
        .setFormat(m_surface_format.format)
        .setSubresourceRange({ vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 })
        .setComponents({ vk::ComponentSwizzle::eR, vk::ComponentSwizzle::eG, vk::ComponentSwizzle::eB, vk::ComponentSwizzle::eA });
    for (int i = 0; i < m_swapchain_resources_list.size(); ++i) {
        auto &resources = m_swapchain_resources_list[i];
        resources.image_sp = VulkanImage::makeShared();
        resources.image_sp->setExtent(vk::Extent3D{ this->getWidth(), this->getHeight(), 1 })
            .setArrayLayers(swapchain_info.imageArrayLayers)
            .setUsage(swapchain_info.imageUsage)
            .setFormat(swapchain_info.imageFormat)
            .create(m_context_p, swapchain_images[i]);
    }
    m_need_to_update = false;
    return true;
}

bool lcf::render::VulkanSwapchain::prepareForRender()
{
    if (not m_surface) { return false; }
    auto physical_device = m_context_p->getPhysicalDevice();
    try {
        m_surface_capabilities = physical_device.getSurfaceCapabilitiesKHR(m_surface);
    } catch (const vk::SystemError &e) {
        return false;
    }
    this->setExtent(m_surface_capabilities.currentExtent.width, m_surface_capabilities.currentExtent.height);
    if (this->getWidth() == 0 or this->getHeight() == 0) { return false; }
    if (m_need_to_update) { this->recreate(); }
    return this->acquireAvailableTarget();
}

void lcf::render::VulkanSwapchain::finishRender(vk::Semaphore present_ready)
{
    m_present_ready = present_ready;
    ++m_target_available_index %= m_swapchain_resources_list.size();
    this->present();
}

void lcf::render::VulkanSwapchain::present()
{
    if (not m_swapchain) { return; }
    auto device = m_context_p->getDevice();
    vk::PresentInfoKHR present_info;
    present_info.setWaitSemaphores(m_present_ready)
        .setSwapchains(m_swapchain.get())
        .setPImageIndices(&m_image_index);
    vk::Result present_result;
    try {
        present_result = m_context_p->getQueue(vk::QueueFlagBits::eGraphics).presentKHR(present_info);
    } catch (const vk::SystemError &e) { }
}

bool lcf::render::VulkanSwapchain::acquireAvailableTarget()
{
    auto device = m_context_p->getDevice();
    vk::Result acquire_result;
    try {
        std::tie(acquire_result, m_image_index) = device.acquireNextImageKHR(m_swapchain.get(), UINT64_MAX, this->getTargetAvailableSemaphore(), nullptr);
    } catch (const vk::SystemError &e) {
        return false;
    }
    if (acquire_result == vk::Result::eErrorOutOfDateKHR) {
        return this->recreate();
    }
    return acquire_result == vk::Result::eSuccess or acquire_result == vk::Result::eSuboptimalKHR;
}
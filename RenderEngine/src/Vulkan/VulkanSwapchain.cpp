#include "VulkanSwapchain.h"
#include "VulkanContext.h"

lcf::render::VulkanSwapchain::VulkanSwapchain(VulkanContext *context, vk::SurfaceKHR surface) :
    RenderTarget(),
    m_context(context),
    m_surface(surface)
{
}

lcf::render::VulkanSwapchain::~VulkanSwapchain()
{
}

void lcf::render::VulkanSwapchain::create()
{
    if (this->isCreated()) { return; }
    vk::PhysicalDevice physical_device = m_context->getPhysicalDevice();
    auto surface_formats = physical_device.getSurfaceFormatsKHR(m_surface);
    auto present_modes = physical_device.getSurfacePresentModesKHR(m_surface);
    vk::SurfaceCapabilitiesKHR surface_capabilities = physical_device.getSurfaceCapabilitiesKHR(m_surface);
    
    vk::SurfaceFormatKHR best_surface_format = surface_formats.front();
    vk::PresentModeKHR best_present_mode = vk::PresentModeKHR::eFifo;
    for (const auto &surface_format : surface_formats) {
        if (surface_format.format == vk::Format::eB8G8R8A8Srgb and surface_format.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear) {
            best_surface_format = surface_format;
            break;
        }
    }
    for (const auto &present_mode : present_modes) {
        if (present_mode == vk::PresentModeKHR::eMailbox) {
            best_present_mode = present_mode;
            break;
        }
    }
    m_surface_format = best_surface_format;
    m_present_mode = best_present_mode;   
}

void lcf::render::VulkanSwapchain::recreate()
{
    vk::Device device = m_context->getDevice();
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
    uint32_t graphics_queue_family_index = m_context->getQueueFamilyIndex(vk::QueueFlagBits::eGraphics);
    uint32_t present_queue_family_index = graphics_queue_family_index;
    swapchain_info.setImageSharingMode(vk::SharingMode::eExclusive)
        .setQueueFamilyIndices(nullptr);
    try {
        m_swapchain = device.createSwapchainKHRUnique(swapchain_info);
    } catch (const vk::SystemError &e) {
        qFatal() << "Failed to create swapchain: " << e.what();
    }

    auto swapchain_images = device.getSwapchainImagesKHR(m_swapchain.get());

    vk::ImageViewCreateInfo image_view_info;
    image_view_info.setViewType(vk::ImageViewType::e2D)
        .setFormat(m_surface_format.format)
        .setSubresourceRange({ vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 })
        .setComponents({ vk::ComponentSwizzle::eR, vk::ComponentSwizzle::eG, vk::ComponentSwizzle::eB, vk::ComponentSwizzle::eA });
    for (int i = 0; i < m_swapchain_resources_list.size(); ++i) {
        auto &resources = m_swapchain_resources_list[i];
        resources.image = swapchain_images[i];
        try {
            resources.image_view = device.createImageViewUnique(image_view_info.setImage(resources.image));
        } catch (const vk::SystemError &e) {
            qFatal() << "Failed to create image view: " << e.what();
        }
    }
    
    m_need_to_update = false;
}

void lcf::render::VulkanSwapchain::destroy()
{
    m_swapchain.reset();
    m_context->getInstance().destroySurfaceKHR(m_surface);
    m_surface = nullptr;
}

bool lcf::render::VulkanSwapchain::prepareForRender(RenderExchangeInfo *exchange_info)
{
    if (not m_surface) { return false; }
    auto physical_device = m_context->getPhysicalDevice();
    m_surface_capabilities = physical_device.getSurfaceCapabilitiesKHR(m_surface);
    if (this->getWidth() == 0 or this->getHeight() == 0) { return false; }
    if (m_need_to_update) { this->recreate(); }
    m_exchange_info = static_cast<VulkanRenderExchangeInfo *>(exchange_info);
    this->acquireAvailableTarget();
    return true;
}

void lcf::render::VulkanSwapchain::finishRender()
{
    auto device = m_context->getDevice();
    vk::PresentInfoKHR present_info;
    vk::Semaphore present_ready = m_exchange_info->getRenderFinishedSemaphore();
    present_info.setWaitSemaphores(present_ready)
        .setSwapchains(m_swapchain.get())
        .setPImageIndices(&m_current_index);
    auto present_result = m_context->getQueue(vk::QueueFlagBits::eGraphics).presentKHR(present_info);
    if (present_result == vk::Result::eErrorOutOfDateKHR or present_result == vk::Result::eSuboptimalKHR) {
        this->recreate();
    } else if (present_result != vk::Result::eSuccess) {
        throw std::runtime_error("lcf::render::VulkanSwapchain::present(): Failed to present swapchain image!");
    }
}

void lcf::render::VulkanSwapchain::acquireAvailableTarget()
{
    auto device = m_context->getDevice();
    vk::Semaphore target_available = m_exchange_info->getTargetAvailableSemaphore();
    auto [acquire_result, index] = device.acquireNextImageKHR(m_swapchain.get(), UINT64_MAX, target_available, nullptr);
    if (acquire_result == vk::Result::eErrorOutOfDateKHR) {
        this->recreate();
    } else if (acquire_result != vk::Result::eSuccess and acquire_result != vk::Result::eSuboptimalKHR) {
        throw std::runtime_error("lcf::render::VulkanSwapchain::acquireAvailableTarget(): Failed to acquire swapchain image!");
    }
    m_current_index = index;
    m_exchange_info->setTargetImage(m_swapchain_resources_list[m_current_index].image)
        .setTargetImageView(m_swapchain_resources_list[m_current_index].image_view.get());
}
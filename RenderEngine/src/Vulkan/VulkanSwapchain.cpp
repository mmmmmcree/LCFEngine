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
    this->destroy();
}

void lcf::render::VulkanSwapchain::create()
{
    if (this->isCreated()) { return; }
    vk::PhysicalDevice physical_device = m_context_p->getPhysicalDevice();
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
    auto device = m_context_p->getDevice();
    vk::SemaphoreCreateInfo semaphore_info;
    for (auto &swapchain_resource : m_swapchain_resources_list) {
        swapchain_resource.target_available = device.createSemaphoreUnique(semaphore_info);
    }
}

void lcf::render::VulkanSwapchain::recreate()
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
    auto instance = m_context_p->getInstance();
    instance.destroySurfaceKHR(m_surface);
    m_surface = nullptr;
}

bool lcf::render::VulkanSwapchain::prepareForRender()
{
    if (not m_surface) { return false; }
    auto physical_device = m_context_p->getPhysicalDevice();
    m_surface_capabilities = physical_device.getSurfaceCapabilitiesKHR(m_surface);
    this->setExtent(m_surface_capabilities.currentExtent.width, m_surface_capabilities.currentExtent.height);
    if (this->getWidth() == 0 or this->getHeight() == 0) { return false; }
    if (m_need_to_update) { this->recreate(); }
    this->acquireAvailableTarget();
    return true;
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
    auto present_result = m_context_p->getQueue(vk::QueueFlagBits::eGraphics).presentKHR(present_info);
    if (present_result == vk::Result::eErrorOutOfDateKHR or present_result == vk::Result::eSuboptimalKHR) {
        this->recreate();
    } else if (present_result != vk::Result::eSuccess) {
        LCF_THROW_RUNTIME_ERROR(std::format(
            "lcf::render::VulkanSwapchain::present(): Failed to present swapchain image! Error code: {}",
            enum_name(present_result)));
    }
}

void lcf::render::VulkanSwapchain::acquireAvailableTarget()
{
    auto device = m_context_p->getDevice();
    auto [acquire_result, index] = device.acquireNextImageKHR(m_swapchain.get(), UINT64_MAX, this->getTargetAvailableSemaphore(), nullptr);
    if (acquire_result == vk::Result::eErrorOutOfDateKHR) {
        this->recreate();
    } else if (acquire_result != vk::Result::eSuccess and acquire_result != vk::Result::eSuboptimalKHR) {
        LCF_THROW_RUNTIME_ERROR(std::format(
            "lcf::render::VulkanSwapchain::acquireAvailableTarget(): Failed to acquire swapchain image! Error code: {}",
            enum_name(acquire_result)));
    }
    m_image_index = index;
}
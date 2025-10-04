#include "VulkanContext.h"
#include <QLoggingCategory>
#include <QDebug>
#include <set>
#include <string>
#include <algorithm>


lcf::render::VulkanContext::VulkanContext()
{
    this->setupVulkanInstance();
}

lcf::render::VulkanContext::~VulkanContext()
{
}

void lcf::render::VulkanContext::registerWindow(RenderWindow * window)
{
    window->setSurfaceType(QSurface::VulkanSurface);
    window->setVulkanInstance(&m_vk_instance);
    window->create();
    vk::SurfaceKHR surface = QVulkanInstance::surfaceForWindow(window); //! surface is available after window is created
    window->setVulkanInstance(nullptr);
    auto render_target = VulkanSwapchain::makeShared(this, surface);
    window->setRenderTarget(render_target);
    m_surface_render_targets.emplace_back(render_target);
}

void lcf::render::VulkanContext::create()
{
    if (this->isCreated()) { return; }
    this->pickPhysicalDevice();
    this->findQueueFamilies();
    this->createLogicalDevice();
    this->createCommandPool();
    m_memory_allocator.create(this);
    SurfaceRenderTargetList{}.swap(m_surface_render_targets);
}

void lcf::render::VulkanContext::setupVulkanInstance()
{
    QByteArrayList extensions = {
    #ifndef NDEBUG
        "VK_EXT_debug_utils",
    #endif
    };
    QByteArrayList layers = {
    #ifndef NDEBUG
        "VK_LAYER_KHRONOS_validation",
    #endif
    };
    m_vk_instance.setApiVersion({1, 3, 2});
    m_vk_instance.setExtensions(extensions);
    m_vk_instance.setLayers(layers);
#ifndef NDEBUG
    qputenv("VK_LAYER_ENABLES", "VK_VALIDATION_FEATURE_ENABLE_DEBUG_PRINTF_EXT");
    qputenv("VK_LAYER_PRINTF_TO_STDOUT", "1"); 
    QVulkanInstance::DebugUtilsFilter debug_utils_filter = [](
        QVulkanInstance::DebugMessageSeverityFlags severity,
        QVulkanInstance::DebugMessageTypeFlags type, const void *message)
    {
        auto* msg_data = static_cast<const VkDebugUtilsMessengerCallbackDataEXT *>(message);
        qDebug() << msg_data->pMessage;
        return true;
    };
    m_vk_instance.installDebugOutputFilter(debug_utils_filter);
    QLoggingCategory::setFilterRules(QStringLiteral("qt.vulkan=true"));
#endif
    if (not m_vk_instance.create()) {
        qFatal("Failed to create Vulkan instance: %d", m_vk_instance.errorCode());
    }
#if ( VULKAN_HPP_DISPATCH_LOADER_DYNAMIC == 1 )
    VULKAN_HPP_DEFAULT_DISPATCHER.init();
    VULKAN_HPP_DEFAULT_DISPATCHER.init(vk::Instance(m_vk_instance.vkInstance()));
#endif
}

void lcf::render::VulkanContext::pickPhysicalDevice()
{
    vk::Instance instance = m_vk_instance.vkInstance();
    auto devices = instance.enumeratePhysicalDevices();
    if (devices.empty()) {
        qFatal() << "No physical devices found";
        return;
    }
    std::ranges::sort(devices, [this](const vk::PhysicalDevice &a, const vk::PhysicalDevice &b) {
        return calculatePhysicalDeviceScore(a) > calculatePhysicalDeviceScore(b);
    });
    m_physical_device = devices.front();
}

int lcf::render::VulkanContext::calculatePhysicalDeviceScore(const vk::PhysicalDevice &device)
{
    vk::PhysicalDeviceProperties properties = device.getProperties();
    vk::PhysicalDeviceFeatures features = device.getFeatures();
    int score = 0;
    score += properties.deviceType == vk::PhysicalDeviceType::eDiscreteGpu ? 1000 : 0;
    score += properties.limits.maxImageDimension2D;
    score += features.geometryShader ? 1000 : 0;
    return score;
}

void lcf::render::VulkanContext::findQueueFamilies()
{
    auto queue_families = m_physical_device.getQueueFamilyProperties();
    vk::QueueFlags required_flags = vk::QueueFlagBits::eGraphics | vk::QueueFlagBits::eCompute;
    bool require_present = not m_surface_render_targets.empty();
    for (int i = 0; i < queue_families.size(); ++i) {
        const auto &queue_family = queue_families[i];
        if (queue_family.queueFlags & (vk::QueueFlagBits::eGraphics | vk::QueueFlagBits::eCompute)) {
            if (require_present) {
                // make sure graphics queue also supports presenting to the surface
                bool supports_all_surfaces = std::ranges::all_of(m_surface_render_targets, [this, i](const auto &render_target) {
                    return m_physical_device.getSurfaceSupportKHR(i, render_target->getSurface());
                });
                if (not supports_all_surfaces) { continue; }
            }
            m_queue_family_indices[static_cast<uint32_t>(vk::QueueFlagBits::eGraphics | vk::QueueFlagBits::eCompute)] = i;
            m_queue_family_indices[static_cast<uint32_t>(vk::QueueFlagBits::eGraphics)] = i;
            m_queue_family_indices[static_cast<uint32_t>(vk::QueueFlagBits::eCompute)] = i;
            required_flags &= ~(vk::QueueFlagBits::eGraphics | vk::QueueFlagBits::eCompute);
        }
        if (not required_flags) { break; }
    }   
}

void lcf::render::VulkanContext::createLogicalDevice()
{
    std::array<float, 1> queue_priorities = { 1.0f };
    std::vector<vk::DeviceQueueCreateInfo> queue_infos(1);
    queue_infos[0].setQueueFamilyIndex(this->getQueueFamilyIndex(vk::QueueFlagBits::eGraphics | vk::QueueFlagBits::eCompute))
        .setQueuePriorities(queue_priorities);

    std::vector<const char *> required_extensions = {
        VK_KHR_MAINTENANCE1_EXTENSION_NAME,
        VK_EXT_DESCRIPTOR_BUFFER_EXTENSION_NAME,
        VK_KHR_TIMELINE_SEMAPHORE_EXTENSION_NAME,
        VK_KHR_SHADER_NON_SEMANTIC_INFO_EXTENSION_NAME,
    };
    if (not m_surface_render_targets.empty()) {
        std::vector<const char *> extensions = {
            VK_KHR_SWAPCHAIN_EXTENSION_NAME,
        };
        required_extensions.insert(required_extensions.end(), extensions.begin(), extensions.end());
    }
    std::set<std::string> required_extensions_set(required_extensions.begin(), required_extensions.end());
    auto available_extensions = m_physical_device.enumerateDeviceExtensionProperties();
    for (const auto &available_extension : available_extensions) {
        required_extensions_set.erase(available_extension.extensionName.data());
    }
    for (const auto &extension : required_extensions_set) {
        qWarning() << "Required extension not available: " << extension;
    }

    vk::StructureChain<vk::DeviceCreateInfo,
        vk::PhysicalDeviceFeatures2,
        vk::PhysicalDeviceVulkan11Features,
        vk::PhysicalDeviceVulkan12Features,
        vk::PhysicalDeviceVulkan13Features> device_info;
    device_info.get<vk::PhysicalDeviceVulkan13Features>().setSynchronization2(true)
        .setDynamicRendering(true);
    device_info.get<vk::PhysicalDeviceVulkan12Features>().setBufferDeviceAddress(true)
        .setDescriptorIndexing(true)
        .setDrawIndirectCount(true)
        .setTimelineSemaphore(true);
    device_info.get<vk::PhysicalDeviceVulkan11Features>().setShaderDrawParameters(true)
        .setStorageBuffer16BitAccess(true);
    device_info.get<vk::PhysicalDeviceFeatures2>().setFeatures(m_physical_device.getFeatures());

    device_info.get<vk::DeviceCreateInfo>().setQueueCreateInfos(queue_infos)
        .setPEnabledExtensionNames(required_extensions);

    try {
        m_device = m_physical_device.createDeviceUnique(device_info.get());
    } catch (const vk::SystemError &e) {
        qFatal() << "Failed to create logical device: " << e.what();
    }
    m_queues[static_cast<uint32_t>(vk::QueueFlagBits::eGraphics | vk::QueueFlagBits::eCompute)] = m_device->getQueue(this->getQueueFamilyIndex(vk::QueueFlagBits::eGraphics | vk::QueueFlagBits::eCompute), 0);
    m_queues[static_cast<uint32_t>(vk::QueueFlagBits::eGraphics)] = m_device->getQueue(this->getQueueFamilyIndex(vk::QueueFlagBits::eGraphics), 0);
    m_queues[static_cast<uint32_t>(vk::QueueFlagBits::eCompute)] = m_device->getQueue(this->getQueueFamilyIndex(vk::QueueFlagBits::eCompute), 0);
}

void lcf::render::VulkanContext::createCommandPool()
{
    vk::CommandPoolCreateInfo command_pool_info;
    command_pool_info.setQueueFamilyIndex(this->getQueueFamilyIndex(vk::QueueFlagBits::eGraphics | vk::QueueFlagBits::eCompute))
        .setFlags(vk::CommandPoolCreateFlagBits::eResetCommandBuffer);
    try {
        m_command_pool = m_device->createCommandPoolUnique(command_pool_info);
    } catch (const vk::SystemError &e) {
        qFatal() << "Failed to create command pool: " << e.what();
    }
}

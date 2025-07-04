#include "VulkanContext.h"
#include <QLoggingCategory>
#include <set>
#include <string>
#include <algorithm>

#include <QDebug>

lcf::render::VulkanContext::VulkanContext()
{
    this->setupVulkanInstance();
}

lcf::render::VulkanContext::~VulkanContext()
{
    for (auto render_target : m_surface_render_targets) {
        delete render_target;
    }
}

void lcf::render::VulkanContext::registerWindow(RenderWindow * window)
{
    window->setSurfaceType(QSurface::VulkanSurface);
    window->setVulkanInstance(&m_vk_instance);
    window->create();
    vk::SurfaceKHR surface = QVulkanInstance::surfaceForWindow(window); //! surface is available after window is created
    auto render_target = new VulkanSwapchain(this, surface);
    window->setRenderTarget(render_target);
    m_surface_render_targets.emplace_back(render_target);
}

void lcf::render::VulkanContext::unregisterWindow(RenderWindow *window)
{
    window->getRenderTarget()->destroy();
    window->setVulkanInstance(nullptr);
}

void lcf::render::VulkanContext::create()
{
    if (this->isCreated()) { return; }
    this->pickPhysicalDevice();
    this->findQueueFamilies();
    this->createLogicalDevice();
    this->createCommandPool();
    m_memory_allocator.create(this);
}

void lcf::render::VulkanContext::setupVulkanInstance()
{
    QVulkanInstance::DebugUtilsFilter debug_utils_filter = [](
        QVulkanInstance::DebugMessageSeverityFlags severity,
        QVulkanInstance::DebugMessageTypeFlags type, const void *message)
    {
        qDebug() << static_cast<const VkDebugUtilsMessengerCallbackDataEXT *>(message)->pMessage;
        return true;
    };
    m_vk_instance.installDebugOutputFilter(debug_utils_filter);
    m_vk_instance.setApiVersion({1, 3, 0});
    m_vk_instance.setLayers({ "VK_LAYER_KHRONOS_validation" });
    QByteArrayList extensions {
        VK_EXT_DEBUG_UTILS_EXTENSION_NAME,
        VK_EXT_DEPTH_RANGE_UNRESTRICTED_EXTENSION_NAME,
    };
    m_vk_instance.setExtensions(extensions);
    QLoggingCategory::setFilterRules(QStringLiteral("qt.vulkan=true"));
    if (not m_vk_instance.create()) {
        qFatal("Failed to create Vulkan instance: %d", m_vk_instance.errorCode());
    }
    vk::DispatchLoaderDynamic loader;
    loader.init();
    loader.init(vk::Instance(m_vk_instance.vkInstance()));
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
    vk::PhysicalDeviceMemoryProperties memory_properties = m_physical_device.getMemoryProperties();
    vk::MemoryPropertyFlags host_visible_memory_flags = vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent;
    for (uint32_t i = 0; i < memory_properties.memoryTypeCount; ++i) {
        auto memory_type = memory_properties.memoryTypes[i];
        if (m_host_visible_memory_index == -1 and (memory_type.propertyFlags & (vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent))) {
            m_host_visible_memory_index = i;
        }
        if (m_device_local_memory_index == -1 and memory_type.propertyFlags & vk::MemoryPropertyFlagBits::eDeviceLocal) {
            m_device_local_memory_index = i;
        }
    }
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

    std::vector<const char *> required_extensions;
    if (not m_surface_render_targets.empty()) {
        std::vector<const char *> extensions = {
            VK_KHR_SWAPCHAIN_EXTENSION_NAME,
            VK_KHR_MAINTENANCE1_EXTENSION_NAME,
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

    vk::PhysicalDeviceVulkan13Features features13;

    features13.setSynchronization2(true)
        .setDynamicRendering(true);
    vk::PhysicalDeviceVulkan12Features features12;
    features12.setBufferDeviceAddress(true)
        .setDescriptorIndexing(true)
        .setPNext(&features13);
    vk::PhysicalDeviceFeatures2 device_features2 = m_physical_device.getFeatures2();
    device_features2.setPNext(&features12);

    vk::DeviceCreateInfo device_info;
    device_info.setQueueCreateInfos(queue_infos)
        .setPEnabledExtensionNames(required_extensions)
        .setPNext(&device_features2);

    try {
        m_device = m_physical_device.createDeviceUnique(device_info);
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

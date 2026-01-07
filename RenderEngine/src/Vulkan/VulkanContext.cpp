#include "Vulkan/VulkanContext.h"
#include "log.h"
#include <set>
#include <string>
#include <algorithm>
#include "gui_types.h"

using namespace lcf::render;

VulkanContext::VulkanContext()
{
    this->setupVulkanInstance();
}

VulkanContext::~VulkanContext()
{
    m_surface_render_targets.clear();
    m_device->waitIdle();
}

VulkanContext & VulkanContext::registerWindow(Entity &window_entity)
{
    auto & bridge_sp = window_entity.getComponent<gui::VulkanSurfaceBridge::SharedPointer>();
    bridge_sp->createBackend(this->getInstance());
    auto render_target = VulkanSwapchain::makeShared(bridge_sp);
    m_surface_render_targets.emplace_back(render_target);
    window_entity.emplaceComponent<VulkanSwapchain::WeakPointer>(render_target); //? maybe variant for headless mode
    return *this;
}

void VulkanContext::create()
{
    if (this->isCreated()) { return; }
    this->pickPhysicalDevice();
    this->findQueueFamilies();
    this->createLogicalDevice();
    this->createCommandPools();
    m_memory_allocator.create(this);
    m_descriptor_set_allocator.create(this);
    m_sampler_manager.create(this);
    for (auto &render_target : m_surface_render_targets) {
        render_target->create(this);
    }
}

uint32_t VulkanContext::getQueueFamilyIndex(vk::QueueFlagBits type) const noexcept
{
    return m_queue_family_indices.at(type);
}

const vk::Queue & VulkanContext::getQueue(vk::QueueFlagBits type) const noexcept
{
    return m_queue_lists.at(type).front();
}

std::span<const vk::Queue> VulkanContext::getSubQueues(vk::QueueFlagBits type) const noexcept
{ 
    return std::span(m_queue_lists.at(type)).subspan(1);
}

const vk::CommandPool &VulkanContext::getCommandPool(vk::QueueFlagBits queue_type) const noexcept
{
    return m_command_pools.at(queue_type).get();
}

#if !defined(NDEBUG) && !VULKAN_HPP_DISPATCH_LOADER_DYNAMIC
PFN_vkCreateDebugUtilsMessengerEXT  pfnVkCreateDebugUtilsMessengerEXT;
PFN_vkDestroyDebugUtilsMessengerEXT pfnVkDestroyDebugUtilsMessengerEXT;

VKAPI_ATTR VkResult VKAPI_CALL vkCreateDebugUtilsMessengerEXT(
    VkInstance instance,
    const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo,
    const VkAllocationCallbacks* pAllocator,
    VkDebugUtilsMessengerEXT* pMessenger)
{
  return pfnVkCreateDebugUtilsMessengerEXT(instance, pCreateInfo, pAllocator, pMessenger);
}

VKAPI_ATTR void VKAPI_CALL vkDestroyDebugUtilsMessengerEXT(
    VkInstance instance,
    VkDebugUtilsMessengerEXT messenger,
    const VkAllocationCallbacks * pAllocator)
{
  return pfnVkDestroyDebugUtilsMessengerEXT(instance, messenger, pAllocator);
}

VKAPI_ATTR vk::Bool32 VKAPI_CALL debug_callback(
    vk::DebugUtilsMessageSeverityFlagBitsEXT severity_flags,
    vk::DebugUtilsMessageTypeFlagsEXT type_flags,
    const vk::DebugUtilsMessengerCallbackDataEXT * callback_data,
    void * user_data);
#endif

void VulkanContext::setupVulkanInstance()
{
#if VULKAN_HPP_DISPATCH_LOADER_DYNAMIC
    static bool s_vulkan_loaded = []() -> bool {
        VULKAN_HPP_DEFAULT_DISPATCHER.init();
        return true;
    }();
#endif
    vk::ApplicationInfo app_info;
    app_info.setPApplicationName("LCFEngine")
        .setPEngineName("LCFEngine")
        .setApplicationVersion(vk::makeVersion(1, 0, 0))
        .setEngineVersion(vk::makeVersion(1, 0, 0))
        .setApiVersion(VK_HEADER_VERSION_COMPLETE);
    std::set<std::string> required_extensions = {
    #ifndef NDEBUG
        VK_EXT_DEBUG_UTILS_EXTENSION_NAME,
    #endif
    };
    auto win_sys_required_extensions = lcf::gui::WindowSystem::getInstance().getRequiredVulkanExtensions();
    required_extensions.insert(win_sys_required_extensions.begin(), win_sys_required_extensions.end());
    if (required_extensions.contains(VK_KHR_SURFACE_EXTENSION_NAME)) {
        std::vector<std::string> surface_required_extensions = {
            VK_KHR_GET_SURFACE_CAPABILITIES_2_EXTENSION_NAME,
            VK_EXT_SURFACE_MAINTENANCE_1_EXTENSION_NAME,
        };
        required_extensions.insert(surface_required_extensions.begin(), surface_required_extensions.end());
    }
    auto available_extensions = vk::enumerateInstanceExtensionProperties();
    std::vector<const char*> extensions;
    for (const auto &extension : available_extensions) {
        if (required_extensions.contains(extension.extensionName.data())) {
            extensions.push_back(extension.extensionName.data());
        }
    }
    std::set<std::string> required_layers = {
    #ifndef NDEBUG
        "VK_LAYER_KHRONOS_validation",
    #endif
    };
    auto available_layers = vk::enumerateInstanceLayerProperties();
    std::vector<const char*> layers;
    for (const auto &layer : available_layers) {
        if (required_layers.contains(layer.layerName.data())) {
            layers.push_back(layer.layerName.data());
        }
    }
    vk::InstanceCreateInfo instance_info;
    instance_info.setPApplicationInfo(&app_info)
        .setPEnabledExtensionNames(extensions)
        .setPEnabledLayerNames(layers);
#ifndef NDEBUG
    std::vector<vk::ValidationFeatureEnableEXT> enabled_validation_features = {
        vk::ValidationFeatureEnableEXT::eDebugPrintf
    };
    vk::ValidationFeaturesEXT validation_features;
    instance_info.setPNext(&validation_features);
#endif
    m_instance = vk::createInstanceUnique(instance_info);
#if VULKAN_HPP_DISPATCH_LOADER_DYNAMIC
    VULKAN_HPP_DEFAULT_DISPATCHER.init(this->getInstance());
#else
    static bool s_dynamic_loaded = [this]() -> bool
    {
    #ifndef NDEBUG
        pfnVkCreateDebugUtilsMessengerEXT = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(m_instance->getProcAddr("vkCreateDebugUtilsMessengerEXT"));
        pfnVkDestroyDebugUtilsMessengerEXT = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(m_instance->getProcAddr("vkDestroyDebugUtilsMessengerEXT"));
    #endif
        return true;
    }();
#endif
#ifndef NDEBUG
    bool is_renderdoc_env = std::getenv("ENABLE_VULKAN_RENDERDOC_CAPTURE"); //! RenderDoc Environment is contradictory to custom debug callback
    if (pfnVkCreateDebugUtilsMessengerEXT and pfnVkDestroyDebugUtilsMessengerEXT and not is_renderdoc_env) {
        vk::DebugUtilsMessengerCreateInfoEXT debug_messenger_info;
        debug_messenger_info.setMessageSeverity(vk::FlagTraits<vk::DebugUtilsMessageSeverityFlagBitsEXT>::allFlags)
            .setMessageType(vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral |
                vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation |
                vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance)
            .setPfnUserCallback(&debug_callback);
        m_debug_messenger = m_instance->createDebugUtilsMessengerEXTUnique(debug_messenger_info);
    }
#endif
}

void VulkanContext::pickPhysicalDevice()
{
    auto devices = m_instance->enumeratePhysicalDevices();
    if (devices.empty()) {
        std::runtime_error error("No physical devices found");
        lcf_log_error(error.what());
        throw error;
    }
    std::ranges::sort(devices, [this](const vk::PhysicalDevice &a, const vk::PhysicalDevice &b) {
        return calculatePhysicalDeviceScore(a) > calculatePhysicalDeviceScore(b);
    });
    m_physical_device = devices.front();
}

int VulkanContext::calculatePhysicalDeviceScore(const vk::PhysicalDevice &device)
{
    vk::PhysicalDeviceProperties properties = device.getProperties();
    vk::PhysicalDeviceFeatures features = device.getFeatures();
    int score = 0;
    score += properties.deviceType == vk::PhysicalDeviceType::eDiscreteGpu ? 1000 : 0;
    score += properties.limits.maxImageDimension2D;
    score += features.geometryShader ? 1000 : 0;
    return score;
}

void VulkanContext::findQueueFamilies()
{
    auto queue_family_properties = m_physical_device.getQueueFamilyProperties();
    auto get_queue_family_index = [this, &queue_family_properties](vk::QueueFlags desired_flags, vk::QueueFlags undesired_flags) -> std::optional<uint32_t>
    {
        std::optional<uint32_t> queue_family_index = std::nullopt;
        for (int i = 0; i < queue_family_properties.size(); ++i) {
            const auto & queue_family_property = queue_family_properties[i];
            if (not (queue_family_property.queueFlags & desired_flags)) { continue; }
            if (desired_flags & vk::QueueFlagBits::eGraphics) {
                bool supports_all_surfaces = std::ranges::all_of(m_surface_render_targets, [this, i](const auto &render_target) {
                    return m_physical_device.getSurfaceSupportKHR(i, render_target->getSurface());
                });
                if (not supports_all_surfaces) { continue; }
            }
            queue_family_index = i;
            if (not (queue_family_property.queueFlags & undesired_flags)) { break; }
        }
        return queue_family_index;
    };
    std::vector<std::pair<vk::QueueFlagBits, vk::QueueFlags>> queue_flags_pairs = {
        { vk::QueueFlagBits::eGraphics, vk::QueueFlagBits {} },
        { vk::QueueFlagBits::eTransfer, vk::QueueFlagBits::eCompute },
        { vk::QueueFlagBits::eCompute, vk::QueueFlagBits::eTransfer },
    };
    for (auto [queue_type, undesired_flags] : queue_flags_pairs) {
        auto queue_family_index = get_queue_family_index(queue_type, undesired_flags);
        if (queue_family_index) {
            m_queue_family_indices[queue_type] = queue_family_index.value();
        }
    }
}

void VulkanContext::createLogicalDevice()
{
    std::vector<const char *> required_extensions = {
        VK_KHR_MAINTENANCE1_EXTENSION_NAME,
        VK_KHR_TIMELINE_SEMAPHORE_EXTENSION_NAME,
        VK_KHR_SHADER_NON_SEMANTIC_INFO_EXTENSION_NAME,
        VK_EXT_DESCRIPTOR_BUFFER_EXTENSION_NAME,
    };
    if (not m_surface_render_targets.empty()) {
        std::vector<const char *> extensions = {
            VK_KHR_SWAPCHAIN_EXTENSION_NAME,
            VK_EXT_SWAPCHAIN_MAINTENANCE_1_EXTENSION_NAME,
        };
        required_extensions.insert(required_extensions.end(), extensions.begin(), extensions.end());
    }
    std::set<std::string> required_extensions_set(required_extensions.begin(), required_extensions.end());
    auto available_extensions = m_physical_device.enumerateDeviceExtensionProperties();
    for (const auto &available_extension : available_extensions) {
        required_extensions_set.erase(available_extension.extensionName.data());
    }
    for (const auto &extension : required_extensions_set) {
        lcf_log_warn("Required extension not available: {}", extension);
    }

    vk::StructureChain<vk::DeviceCreateInfo,
        vk::PhysicalDeviceFeatures2,
        vk::PhysicalDeviceVulkan11Features,
        vk::PhysicalDeviceVulkan12Features,
        vk::PhysicalDeviceVulkan13Features,
        vk::PhysicalDeviceSwapchainMaintenance1FeaturesEXT> device_info;
    device_info.get<vk::PhysicalDeviceSwapchainMaintenance1FeaturesEXT>().setSwapchainMaintenance1(true);
    device_info.get<vk::PhysicalDeviceVulkan13Features>().setSynchronization2(true)
        .setDynamicRendering(true);
    device_info.get<vk::PhysicalDeviceVulkan12Features>().setBufferDeviceAddress(true)
        .setDescriptorIndexing(true) 
        .setDescriptorBindingVariableDescriptorCount(true)
        .setDescriptorBindingPartiallyBound(true)
        .setDescriptorBindingUpdateUnusedWhilePending(true)
        .setDescriptorBindingSampledImageUpdateAfterBind(true)
        .setRuntimeDescriptorArray(true)
        .setShaderSampledImageArrayNonUniformIndexing(true)
        .setDrawIndirectCount(true)
        .setTimelineSemaphore(true);
    device_info.get<vk::PhysicalDeviceVulkan11Features>().setShaderDrawParameters(true)
        .setStorageBuffer16BitAccess(true);
    device_info.get<vk::PhysicalDeviceFeatures2>().setFeatures(m_physical_device.getFeatures());

    static const std::unordered_map<vk::QueueFlagBits, uint32_t> s_ideal_queue_counts {
        { vk::QueueFlagBits::eGraphics, 2 },
        { vk::QueueFlagBits::eCompute, 2 },
        { vk::QueueFlagBits::eTransfer, 1 },
    };
    auto queue_family_properties = m_physical_device.getQueueFamilyProperties();
    std::unordered_map<uint32_t, std::vector<float>> index_to_priorities;
    for (auto [queue_flags, queue_index] : m_queue_family_indices) {
        uint32_t ideal_queue_count = s_ideal_queue_counts.at(queue_flags);
        uint32_t available_queue_count = queue_family_properties[queue_index].queueCount;
        uint32_t queue_count = std::min(ideal_queue_count, available_queue_count);
        auto & priorities = index_to_priorities[queue_index];
        for (size_t i = priorities.size(); i < queue_count; ++i) {
            priorities.push_back(1.0f - i / static_cast<float>(ideal_queue_count));
        }
    }
    std::vector<vk::DeviceQueueCreateInfo> queue_infos;
    for (auto & [index, priorities] : index_to_priorities) {
        vk::DeviceQueueCreateInfo queue_info;
        queue_info.setQueueFamilyIndex(index)
            .setQueuePriorities(priorities);
        queue_infos.emplace_back(queue_info);
    }
    device_info.get<vk::DeviceCreateInfo>().setQueueCreateInfos(queue_infos)
        .setPEnabledExtensionNames(required_extensions);

    try {
        m_device = m_physical_device.createDeviceUnique(device_info.get());
    } catch (const vk::SystemError &e) {
        lcf_log_error(e.what());
    }

    for (auto [queue_type, queue_index] : m_queue_family_indices) {
        auto & queue_list = m_queue_lists[queue_type];
        uint32_t queue_count = index_to_priorities[queue_index].size();
        queue_list.reserve(queue_count);
        for (uint32_t i = 0; i < queue_count; ++i) {
            queue_list.emplace_back(m_device->getQueue(queue_index, i));
        }
    }
}

void VulkanContext::createCommandPools()
{
    for (auto [queue_type, queue_index] : m_queue_family_indices) {
        vk::CommandPoolCreateInfo command_pool_info;
        command_pool_info.setQueueFamilyIndex(queue_index)
            .setFlags(vk::CommandPoolCreateFlagBits::eResetCommandBuffer);
        try {
            m_command_pools.emplace(std::make_pair(queue_type, m_device->createCommandPoolUnique(command_pool_info)));
        } catch (const vk::SystemError &e) {
            lcf_log_error(e.what());
        }
    }
}

#if !defined(NDEBUG) && !VULKAN_HPP_DISPATCH_LOADER_DYNAMIC
VKAPI_ATTR vk::Bool32 VKAPI_CALL debug_callback(
    vk::DebugUtilsMessageSeverityFlagBitsEXT severity_flags,
    vk::DebugUtilsMessageTypeFlagsEXT type_flags,
    const vk::DebugUtilsMessengerCallbackDataEXT * callback_data,
    void * user_data)
{
    if (not callback_data->pMessage) { return false; }
    std::string log_output = std::format(
        "{:=^80}\n"
        "[Type] {}\n"
        "[Message ID] (0x{:x}) {}\n"
        "[Message] {}\n",
        "Vulkan Validation Layer",
        vk::to_string(type_flags),
        static_cast<uint32_t>(callback_data->messageIdNumber),
        callback_data->pMessageIdName,
        callback_data->pMessage);
    if (callback_data->queueLabelCount > 0) {
        log_output += std::format("<Queue Labels>\n");
        for (uint32_t i = 0; i < callback_data->queueLabelCount; ++i) {
            const auto & label = callback_data->pQueueLabels[i];
            log_output += std::format(
                "  [{}] Name: {}, Color: [{:.3f}, {:.3f}, {:.3f}, {:.3f}]\n",
                i,
                label.pLabelName ? label.pLabelName : "Unnamed",
                label.color[0], label.color[1], label.color[2], label.color[3]);
        }
    }
    if (callback_data->cmdBufLabelCount > 0) {
        log_output += std::format("<Command Buffer Labels>\n");
        for (uint32_t i = 0; i < callback_data->cmdBufLabelCount; ++i) {
            const auto & label = callback_data->pCmdBufLabels[i];
            log_output += std::format(
                "  [{}] Name: {}, Color: [{:.3f}, {:.3f}, {:.3f}, {:.3f}]\n",
                i,
                label.pLabelName ? label.pLabelName : "Unnamed",
                label.color[0], label.color[1], label.color[2], label.color[3]);
        }
    }
    if (callback_data->objectCount > 0) {
        log_output += std::format("<Related Objects>\n");
        for (uint32_t i = 0; i < callback_data->objectCount; ++i) {
            const auto & obj = callback_data->pObjects[i];
            log_output += std::format(
                "  [{}] Type: {}, Handle: 0x{:x}, Name: {}\n",
                i,
                vk::to_string(obj.objectType),
                obj.objectHandle,
                obj.pObjectName ? obj.pObjectName : "Unnamed");
        }
    }
    if (severity_flags & vk::DebugUtilsMessageSeverityFlagBitsEXT::eError) { lcf_log_error(log_output); }
    else if (severity_flags & vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning) { lcf_log_warn(log_output); }
    // else if (severity_flags & vk::DebugUtilsMessageSeverityFlagBitsEXT::eInfo) { lcf_log_info(log_output); }
    // else if (severity_flags & vk::DebugUtilsMessageSeverityFlagBitsEXT::eVerbose) { lcf_log_trace(log_output); }
    return false;
}
#endif
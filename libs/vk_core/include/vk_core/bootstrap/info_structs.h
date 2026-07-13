#pragma once

#include <vulkan/vulkan.hpp>
#include <unordered_set>
#include <unordered_map>
#include <optional>
#include <functional>
#include "concepts/range_concept.h"
#include "resource_utils.h"

namespace lcf::vkc {

class InstanceExtensionManifest;

class DeviceExtensionManifest ;

} // namespace lcf::vkc::mnf

namespace lcf::vkc::utils {

struct PhysicalDeviceFeatureBit;

} // namespace lcf::vkc::utils

namespace lcf::vkc::bs {

class InstanceCreateInfo 
{
    using Self = InstanceCreateInfo;
    using StringSet = std::unordered_set<std::string>;
    using ResourceLeaseList = std::vector<ResourceLease>;
    using ExtEnableCallback = std::function<ResourceLeaseList(vk::Instance)>;
public:
    ~InstanceCreateInfo() noexcept = default;
    InstanceCreateInfo() noexcept = default;
    InstanceCreateInfo(const Self &) = default;
    InstanceCreateInfo(Self &&) noexcept = default;
    Self & operator =(const Self &) = default;
    Self & operator =(Self &&) noexcept = default;
public:
    Self & setApplicationInfo(const vk::ApplicationInfo & application_info) noexcept
    {
        m_application_info = application_info;
        return *this;
    }
    Self & addRequiredInstanceLayer(std::string_view instance_layer) noexcept
    {
        m_required_instance_layers.insert(std::string(instance_layer));
        return *this;
    }
    Self & addRequiredInstanceLayers(convertible_range_of_c<std::string> auto && instance_layers) noexcept
    {
        m_required_instance_layers.insert_range(instance_layers);
        return *this;
    }
    Self & setRequiredInstanceExtensionManifest(const InstanceExtensionManifest & extension_manifest) noexcept
    {
        m_extension_manifest_p = &extension_manifest;
        return *this;
    }
    const vk::ApplicationInfo & getApplicationInfo() const noexcept { return m_application_info; }
    bool isLayerRequired(const std::string & layer_name) const noexcept { return m_required_instance_layers.contains(layer_name); }
    bool isExtensionRequired(const std::string & extension_name) const noexcept;
    std::size_t getRequiredInstanceLayerCount() const noexcept { return m_required_instance_layers.size(); }
    std::size_t getRequiredInstanceExtensionCount() const noexcept;
    ExtEnableCallback getExtensionEnableCallback() const noexcept;
    void printUnsupportedExtensions() const noexcept;
    void printUnsupportedLayers() const noexcept;
private:
    vk::ApplicationInfo m_application_info;
    StringSet m_required_instance_layers;
    const InstanceExtensionManifest * m_extension_manifest_p = nullptr;
};

class PhysicalDeviceSelectInfo
{
    using Self = PhysicalDeviceSelectInfo;
public:
    ~PhysicalDeviceSelectInfo() noexcept = default;
    PhysicalDeviceSelectInfo() noexcept = default;
    PhysicalDeviceSelectInfo(const Self &) = default;
    PhysicalDeviceSelectInfo(Self &&) noexcept = default;
    Self & operator =(const Self &) = default;
    Self & operator =(Self &&) noexcept = default;
public:
    Self & setPreferredType(vk::PhysicalDeviceType type) noexcept
    {
        m_preferred_type = type;
        return *this;
    }
    Self & setRequiredDeviceExtensionManifest(const DeviceExtensionManifest & manifest) noexcept
    {
        m_extension_manifest_p = &manifest;
        return *this;
    }
    Self & addRequiredQueueFlags(vk::QueueFlags flags) noexcept
    {
        m_required_queue_flags |= flags;
        return *this;
    }
    const std::optional<vk::PhysicalDeviceType> & getPreferredTypeOptional() const noexcept { return m_preferred_type; }
    vk::QueueFlags getRequiredQueueFlags() const noexcept { return m_required_queue_flags; }
    
    bool isRequiredFeaturesSupported(vk::PhysicalDevice physical_device) const noexcept;
    bool isExtensionRequired(const std::string & extension_name) const noexcept;
    std::size_t getRequiredDeviceExtensionCount() const noexcept;
private:
    std::optional<vk::PhysicalDeviceType> m_preferred_type;
    vk::QueueFlags m_required_queue_flags = {};
    const DeviceExtensionManifest * m_extension_manifest_p = nullptr;
};

using QueueFamilyRequest = std::pair<uint32_t, float>; //- {family_index, priority}

class DeviceCreateInfo
{
    using Self = DeviceCreateInfo;
    using PriorityList = std::vector<float>;
    using QueueFamilyRequestMap = std::unordered_map<uint32_t, PriorityList>;
public:
    ~DeviceCreateInfo() noexcept = default;
    DeviceCreateInfo() noexcept = default;
    DeviceCreateInfo(const Self &) = default;
    DeviceCreateInfo(Self &&) noexcept = default;
    Self & operator =(const Self &) = default;
    Self & operator =(Self &&) noexcept = default;
public:
    Self & setRequiredDeviceExtensionManifest(const DeviceExtensionManifest & manifest) noexcept
    {
        m_extension_manifest_p = &manifest;
        return *this;
    }
    Self & addQueueFamilyRequest(const QueueFamilyRequest & request) noexcept
    {
        m_queue_family_requests[request.first].emplace_back(request.second);
        return *this;
    }
    Self & addQueueFamilyRequests(convertible_range_of_c<QueueFamilyRequest> auto && requests) noexcept
    {
        for (const auto & request : requests) {
            m_queue_family_requests[request.first].emplace_back(request.second);
        }
        return *this;
    }
    const QueueFamilyRequestMap & getQueueFamilyRequests() const noexcept { return m_queue_family_requests; }
    bool isExtensionRequired(const std::string & extension_name) const noexcept;
    bool isFeatureRequired(const utils::PhysicalDeviceFeatureBit & feature_bit) const noexcept;
    std::size_t getRequiredDeviceExtensionCount() const noexcept;
    const vk::PhysicalDeviceFeatures2 * getRequiredFeatures() const noexcept;
    bool isRequiredFeaturesSupported(vk::PhysicalDevice physical_device) const noexcept;
    void printUnsupportedExtensions(vk::PhysicalDevice physical_device) const noexcept;
    void printUnsupportedFeatures(vk::PhysicalDevice physical_device) const noexcept;
private:
    const DeviceExtensionManifest * m_extension_manifest_p = nullptr;
    QueueFamilyRequestMap m_queue_family_requests;
};

} // namespace lcf::vkc::bs
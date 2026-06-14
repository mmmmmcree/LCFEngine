#pragma once

#include <vulkan/vulkan.hpp>
#include <unordered_set>
#include <optional>
#include "concepts/range_concept.h"

namespace lcf::vkc {

class InstanceExtensionManifest;

class DeviceExtensionManifest ;

} // namespace lcf::vkc::mnf

namespace lcf::vkc::bs {

class InstanceCreateInfo 
{
    using Self = InstanceCreateInfo;
    using StringSet = std::unordered_set<std::string>;
public:
    ~InstanceCreateInfo() noexcept;
    InstanceCreateInfo() noexcept = default;
    InstanceCreateInfo(const Self &) = delete;
    InstanceCreateInfo(Self &&) noexcept = default;
    Self & operator =(const Self &) = delete;
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
    Self & setRequiredInstanceExtensionManifest(const InstanceExtensionManifest & extension_manifest) noexcept;

    const vk::ApplicationInfo & getApplicationInfo() const noexcept { return m_application_info; }
    bool isLayerRequired(const std::string & layer_name) const noexcept { return m_required_instance_layers.contains(layer_name); }
    bool isExtensionRequired(const std::string & extension_name) const noexcept;
private:
    vk::ApplicationInfo m_application_info;
    StringSet m_required_instance_extensions;
    StringSet m_required_instance_layers;
    const InstanceExtensionManifest * m_extension_manifest_p = nullptr;
};

class PhysicalDeviceSelectInfo 
{
    using Self = PhysicalDeviceSelectInfo;
public:
    ~PhysicalDeviceSelectInfo() noexcept = default;
    PhysicalDeviceSelectInfo() noexcept = default;
    PhysicalDeviceSelectInfo(const Self &) = delete;
    PhysicalDeviceSelectInfo(Self &&) noexcept = default;
    Self & operator =(const Self &) = delete;
    Self & operator =(Self &&) noexcept = default;
public:
    
private:
    vk::PhysicalDeviceType m_preffered_type;
};

class DeviceCreateInfo 
{
    using Self = DeviceCreateInfo;
public:
    ~DeviceCreateInfo() noexcept = default;
    DeviceCreateInfo() noexcept = default;
    DeviceCreateInfo(const Self &) = delete;
    DeviceCreateInfo(Self &&) noexcept = default;
    Self & operator =(const Self &) = delete;
    Self & operator =(Self &&) noexcept = default;
public:
    
private:
};

} // namespace lcf::vkc::bs
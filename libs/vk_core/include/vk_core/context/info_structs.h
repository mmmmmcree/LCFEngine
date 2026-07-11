#pragma once

#include <vulkan/vulkan.hpp>
#include "vk_core/bootstrap/info_structs.h"

namespace lcf::vkc {

using InstanceContextCreateInfo = bs::InstanceCreateInfo;

class DeviceContextCreateInfo
{
    using Self = DeviceContextCreateInfo;
public:
    ~DeviceContextCreateInfo() noexcept = default;
    DeviceContextCreateInfo() noexcept = default;
    DeviceContextCreateInfo(const Self &) = delete;
    DeviceContextCreateInfo(Self &&) noexcept = default;
    Self & operator =(const Self &) = delete;
    Self & operator =(Self &&) noexcept = default;
public:
    Self & setPhysicalDeviceSelectInfo(const bs::PhysicalDeviceSelectInfo & info) noexcept
    {
        m_physical_device_select_info = info;
        return *this;
    }
    Self & setRequiredDeviceExtensionManifest(const DeviceExtensionManifest & manifest) noexcept
    {
        m_device_create_info.setRequiredDeviceExtensionManifest(manifest);   
        return *this;
        
    }
    const bs::PhysicalDeviceSelectInfo & getPhysicalDeviceSelectInfo() const noexcept { return m_physical_device_select_info; }
    const bs::DeviceCreateInfo & getDeviceCreateInfo() const noexcept { return m_device_create_info; }   
private:
    bs::PhysicalDeviceSelectInfo m_physical_device_select_info;
    bs::DeviceCreateInfo m_device_create_info;
};

} // namespace lcf::vkc

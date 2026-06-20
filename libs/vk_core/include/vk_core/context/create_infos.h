#pragma once

#include <vulkan/vulkan.hpp>
#include <array>
#include <optional>
#include "vk_core/bootstrap/create_infos.h"
#include "concepts/range_concept.h"
#include "enums.h"
#include "enums/enum_count.h"

namespace lcf::vkc {

using InstanceContextCreateInfo = bs::InstanceCreateInfo;

class DeviceContextCreateInfo
{
    friend class DeviceContext;
    friend class RenderDeviceContext;
    using Self = DeviceContextCreateInfo;
    using QueueFamilyRequest = std::pair<uint32_t, uint32_t>;   // {family_index, queue_count}
public:
    ~DeviceContextCreateInfo() noexcept = default;
    DeviceContextCreateInfo() noexcept = default;
    DeviceContextCreateInfo(const Self &) = default;
    DeviceContextCreateInfo(Self &&) noexcept = default;
    Self & operator =(const Self &) = default;
    Self & operator =(Self &&) noexcept = default;
public:
    Self & setPhysicalDeviceSelectInfo(const bs::PhysicalDeviceSelectInfo & info) noexcept
    {
        m_physical_device_select_info = info;
        return *this;
    }
    Self & setDeviceRole(DeviceRole role) noexcept
    {
        m_device_role = role;
        return *this;   
    }
    Self & setRequiredDeviceExtensionManifest(const DeviceExtensionManifest & manifest) noexcept
    {
        m_device_create_info.setRequiredDeviceExtensionManifest(manifest);   
        return *this;
        
    }
    const DeviceRole & getDeviceRole() const noexcept { return m_device_role; }   
    const bs::PhysicalDeviceSelectInfo & getPhysicalDeviceSelectInfo() const noexcept { return m_physical_device_select_info; }
    const bs::DeviceCreateInfo & getDeviceCreateInfo() const noexcept { return m_device_create_info; }   
private:
    Self & addQueueFamilyRequest(const QueueFamilyRequest & request) noexcept
    {
        m_device_create_info.addQueueFamilyRequest(request);   
        return *this;   
    }
    Self & addQueueFamilyRequests(convertible_range_of_c<QueueFamilyRequest> auto && requests) noexcept
    {
        m_device_create_info.addQueueFamilyRequests(std::forward<decltype(requests)>(requests));   
        return *this;
    }
private:
    DeviceRole m_device_role = DeviceRole::eMain;;
    bs::PhysicalDeviceSelectInfo m_physical_device_select_info;
    bs::DeviceCreateInfo m_device_create_info;
};

} // namespace lcf::vkc

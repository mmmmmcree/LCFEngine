#pragma once

#include <vulkan/vulkan.hpp>

namespace lcf::vkc {

class DeviceContext
{
    using Self = DeviceContext;
public:
    ~DeviceContext() noexcept = default;
    DeviceContext(vk::PhysicalDevice physical_device, vk::UniqueDevice device) noexcept :
        m_physical_device(physical_device),
        m_device(std::move(device)) {}
    DeviceContext(const Self &) = delete;
    DeviceContext(Self &&) = default;
    Self & operator =(const Self &) = delete;
    Self & operator =(Self &&) = default;
public:
    const vk::PhysicalDevice & getPhysicalDevice() const noexcept { return m_physical_device; }
    const vk::Device & getDevice() const noexcept { return m_device.get(); }
private:
    vk::PhysicalDevice m_physical_device;
    vk::UniqueDevice m_device;
};

} // namespace lcf::vkc
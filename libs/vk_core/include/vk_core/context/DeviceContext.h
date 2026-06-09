#pragma once

#include <vulkan/vulkan.hpp>

namespace lcf::vkc {

class DeviceContext
{
    using Self = DeviceContext;
public:
    ~DeviceContext() noexcept = default;
    DeviceContext(const Self &) = delete;
    DeviceContext(Self &&) = default;
    Self & operator =(const Self &) = delete;
    Self & operator =(Self &&) = default;
public:
    const vk::PhysicalDevice & getPhysicalDevice() const noexcept { return m_physical_device; }
    const vk::Device & getDevice() const noexcept { return m_device; }
private:
    vk::PhysicalDevice m_physical_device;
    vk::Device m_device;
};

} // namespace lcf::vkc
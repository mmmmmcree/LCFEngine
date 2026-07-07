#pragma once

#include <vulkan/vulkan.hpp>
#include <type_traits>
#include "resource_utils.h"

namespace lcf::vkc::details {

template <typename Handle>
requires std::is_same_v<Handle, vk::Image> or std::is_same_v<Handle, vk::Buffer>
class Memory;

} // namespace lcf::vkc::details

namespace lcf::vkc {

class Buffer
{
    using Self = Buffer;
    using Memory = details::Memory<vk::Buffer>;
public:
    ~Buffer() noexcept = default;
    Buffer() = default;
    Buffer(Memory && memory, vk::DeviceAddress device_address = 0) noexcept;
    Buffer(const Self &) noexcept = default;
    Self & operator=(const Self &) noexcept = default;
    Buffer(Self &&) noexcept = default;
    Self & operator=(Self &&) noexcept = default;
    operator vk::Buffer() const noexcept;
public:
    const vk::Buffer & handle() const noexcept;
    ResourceLease lease() const noexcept;
    const vk::DeviceAddress & getDeviceAddress() const noexcept { return m_device_address; }
private:
    ResourcePtr<Memory> m_memory_rp;
    vk::DeviceAddress m_device_address = 0;
};

} // namespace lcf::vkc

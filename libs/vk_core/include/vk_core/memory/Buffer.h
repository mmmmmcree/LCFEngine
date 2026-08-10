#pragma once

#include <vulkan/vulkan.hpp>
#include <type_traits>
#include "resource_utils.h"
#include "vk_core/utils/ResourceHandle.h"
#include "vk_core/memory/details/Memory.h"

namespace lcf::vkc {

class Buffer
{
    using Self = Buffer;
    using Memory = details::Memory<vk::Buffer>;
public:
    ~Buffer() noexcept = default;
    Buffer() = default;
    Buffer(utils::ResourceHandle<Memory> memory_rh, vk::DeviceAddress device_address = 0) noexcept :
        m_memory_rh(std::move(memory_rh)),
        m_device_address(device_address) {}
    Buffer(const Self &) noexcept = default;
    Self & operator=(const Self &) noexcept = default;
    Buffer(Self &&) noexcept = default;
    Self & operator=(Self &&) noexcept = default;
    operator const vk::Buffer &() const noexcept { return this->handle(); }
public:
    const vk::Buffer & handle() const noexcept { return m_memory_rh->handle(); }
    ResourceLease lease() const noexcept { return m_memory_rh.lease(); }
    const vk::DeviceAddress & getDeviceAddress() const noexcept { return m_device_address; }
private:
    utils::ResourceHandle<Memory> m_memory_rh;
    vk::DeviceAddress m_device_address = 0;
};

} // namespace lcf::vkc

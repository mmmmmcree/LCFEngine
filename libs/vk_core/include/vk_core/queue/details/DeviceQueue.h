#pragma once

#include <vulkan/vulkan.hpp>

namespace lcf::vkc::details {

class DeviceQueue
{
    using Self = DeviceQueue;
public:
    enum class SharingMode
    {
        eExclusive,
        eShared
    };
public:
    ~DeviceQueue() noexcept = default;
    DeviceQueue(
        vk::Device device,
        uint32_t family_index,
        uint32_t queue_index,
        SharingMode sharing_mode = SharingMode::eExclusive) noexcept :
        m_device(device), m_family_index(family_index)
    {
        m_queue = device.getQueue(family_index, queue_index);
        if (sharing_mode == SharingMode::eShared) { m_mutex_opt.emplace(); }
    }
    DeviceQueue(const Self &) = delete;
    DeviceQueue(Self &&) = delete;
    Self &operator=(const Self &) = delete;
    Self &operator=(Self &&) = delete;
public:
    const vk::Device & getDevice() const noexcept { return m_device; }
    const vk::Queue & getQueue() const noexcept { return m_queue; }
    const uint32_t & getFamilyIndex() const noexcept { return m_family_index; }
    std::optional<std::mutex> & getMutexOpt() const noexcept { return m_mutex_opt; }
private:
    vk::Device m_device = nullptr;
    vk::Queue m_queue = nullptr;
    uint32_t m_family_index = 0u;
    mutable std::optional<std::mutex> m_mutex_opt;
};

} // namespace lcf::vkc::details
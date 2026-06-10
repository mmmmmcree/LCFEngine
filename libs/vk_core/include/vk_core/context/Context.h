#pragma once

#include "vk_core/context/DeviceContext.h"
#include <span>
#include <vector>

namespace lcf::vkc {

class Context
{
    using Self = Context;
public:
    Context() = default;
    ~Context() noexcept = default;
    Context(const Self &) = delete;
    Context(Self &&) noexcept = default;
    Self & operator =(const Self &) = delete;
    Self & operator =(Self &&) noexcept = default;

    std::span<DeviceContext> deviceContexts() noexcept { return m_device_contexts; }
    DeviceContext & getDeviceContext(enums::DeviceRole role) noexcept
    {
        return *m_device_table[std::to_underlying(role)];
    }

private:
    vk::UniqueInstance m_instance;
    // debug messenger, instance-level dispatch
    std::vector<DeviceContext> m_device_contexts; // ownership, sorted by bootstrap's scorer
    // role index built by bootstrap: eMain = strongest by scorer; eCompute = best
    // remaining compute-capable device, aliases eMain on single-GPU machines
    std::array<DeviceContext *, enum_count_v<enums::DeviceRole>> m_device_table {};
};

} // namespace lcf::vkc

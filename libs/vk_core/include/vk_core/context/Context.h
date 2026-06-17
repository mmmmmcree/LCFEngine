#pragma once

#include <vector>
#include <array>
#include "enums.h"
#include "enums/enum_count.h"
#include "resource_utils.h"

namespace lcf::vkc {

class ContextCreateInfo;

class DeviceContext;

class Context
{
    using Self = Context;
    using ResourceLeaseList = std::vector<ResourceLease>;
    using DeviceContextUP = std::unique_ptr<DeviceContext>;
public:
    Context() = default;
    ~Context() noexcept = default;
    Context(const Self &) = delete;
    Context(Self &&) noexcept = default;
    Self & operator =(const Self &) = delete;
    Self & operator =(Self &&) noexcept = default;
public:
    std::error_code create(const ContextCreateInfo & create_info) noexcept;
    const vk::Instance & getInstance() const noexcept { return m_instance.get(); }
    const DeviceContext & getDeviceContext(enums::DeviceRole role) const noexcept { return *m_device_table[std::to_underlying(role)]; }
private:
    vk::UniqueInstance m_instance;
    ResourceLeaseList m_extension_resource_leases;
    std::vector<DeviceContextUP> m_device_contexts;
    std::array<DeviceContext *, enum_count_v<enums::DeviceRole>> m_device_table {};
};

} // namespace lcf::vkc

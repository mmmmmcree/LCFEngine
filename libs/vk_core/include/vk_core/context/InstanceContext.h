#pragma once

#include <vulkan/vulkan.hpp>
#include "resource_utils.h"

namespace lcf::vkc {
namespace bs {

class InstanceCreateInfo;

} // namespace lcf::vkc::bs

class DeviceContext;

class InstanceContext
{
    using Self = InstanceContext;
    using ResourceLeaseList = std::vector<ResourceLease>;
public:
    ~InstanceContext() noexcept = default;
    InstanceContext() = default;
    InstanceContext(const Self &) = delete;
    InstanceContext(Self &&) noexcept = default;
    Self & operator =(const Self &) = delete;
    Self & operator =(Self &&) noexcept = default;
public:
    std::error_code create(const bs::InstanceCreateInfo & instance_info) noexcept;
    const vk::Instance & getInstance() const noexcept { return m_instance.get(); }
private:
    vk::UniqueInstance m_instance;
    ResourceLeaseList m_ext_resource_leases;
};

} // namespace lcf::vkc
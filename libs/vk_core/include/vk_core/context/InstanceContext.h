#pragma once

#include <vulkan/vulkan.hpp>
#include "resource_utils.h"
#include "vk_core/error.h"

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
    //! @param warning_out optional; receives a non-fatal diagnostic (missing instance layers) when one occurs.
    Error create(const bs::InstanceCreateInfo & instance_info, Error * warning_out = nullptr) noexcept;
    const vk::Instance & getInstance() const noexcept { return m_instance.get(); }
private:
    vk::UniqueInstance m_instance;
    ResourceLeaseList m_ext_resource_leases;
};

} // namespace lcf::vkc
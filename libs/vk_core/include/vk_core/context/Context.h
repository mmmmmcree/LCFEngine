#pragma once

#include "vk_core/context/DeviceContext.h"
#include "concepts/range_concept.h"
#include "enums.h"
#include "resource_utils.h"
#include <span>
#include <vector>
#include <unordered_set>

namespace lcf::vkc {

class ContextCreateInfo;

class Context
{
    using Self = Context;
    using ResourceLeaseList = std::vector<ResourceLease>;
public:
    Context() = default;
    ~Context() noexcept = default;
    Context(const Self &) = delete;
    Context(Self &&) noexcept = default;
    Self & operator =(const Self &) = delete;
    Self & operator =(Self &&) noexcept = default;
public:
    std::error_code create(const ContextCreateInfo & create_info) noexcept;
    const DeviceContext & getDeviceContext(enums::DeviceRole role) const noexcept
    {
        return *m_device_table[std::to_underlying(role)];
    }
    const vk::Instance & getInstance() const noexcept { return m_instance.get(); }
private:
    vk::UniqueInstance m_instance;
    ResourceLeaseList m_extension_resource_leases;
    std::vector<DeviceContext> m_device_contexts;
    std::array<DeviceContext *, enum_count_v<enums::DeviceRole>> m_device_table {};
};


class ContextCreateInfo
{
    using Self = ContextCreateInfo;
    using StringSet = std::unordered_set<std::string>;
public:
    Self & setApplicationInfo(const vk::ApplicationInfo & application_info) noexcept
    {
        m_application_info = application_info;
        return *this;
    }
    Self & addRequiredInstanceExtensions(convertible_range_of_c<std::string> auto && instance_extensions) noexcept
    {
        m_required_instance_extensions.insert_range(instance_extensions);
        return *this;
    }
    Self & addRequiredInstanceLayers(convertible_range_of_c<std::string> auto && instance_layers) noexcept
    {
        m_required_instance_layers.insert_range(instance_layers);
        return *this;
    }
    Self & addCapabilitiesFlags(enums::ContextCapabilitiesFlags capabilities_flags) noexcept
    {
        m_capabilities_flags |= capabilities_flags;
        return *this;
    }

    const vk::ApplicationInfo & getApplicationInfo() const noexcept { return m_application_info; }
    bool requiresLayer(const std::string & layer_name) const noexcept { return m_required_instance_extensions.contains(layer_name); }
    bool requiresExtension(const std::string & extension_name) const noexcept { return m_required_instance_extensions.contains(extension_name); }
private:
    vk::ApplicationInfo m_application_info;
    enums::ContextCapabilitiesFlags m_capabilities_flags = enums::ContextCapabilitiesFlags::eNone;
    StringSet m_required_instance_extensions;
    StringSet m_required_instance_layers;
};

} // namespace lcf::vkc

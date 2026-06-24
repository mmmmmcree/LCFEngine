#pragma once

#include <vulkan/vulkan.hpp>
#include <unordered_set>
#include "concepts/range_concept.h"                                                               
#include "resource_utils.h"    

namespace lcf::vkc {

class InstanceExtensionManifest
{
    using Self = InstanceExtensionManifest;
    using StringSet = std::unordered_set<std::string>;
    using ExtEnableCallback = std::function<ResourceLease(vk::Instance)>;
    using ExtEnableCallbackList = std::vector<ExtEnableCallback>; 
    using ResourceLeaseList = std::vector<ResourceLease>;
public:
    Self & addRequiredExtension(std::string_view extension_name) noexcept
    {
        m_required_extensions.insert(std::string(extension_name));
        return *this;
    }
    Self & addRequiredExtensions(convertible_range_of_c<std::string> auto && extension_names) noexcept
    {
        m_required_extensions.insert_range(std::forward<decltype(extension_names)>(extension_names));
        return *this;
    }
    Self & addExtensionEnableCallback(ExtEnableCallback callback) noexcept
    {
        m_ext_enable_callbacks.emplace_back(std::move(callback));
        return *this;
    }
    bool isExtensionRequired(const std::string & extension_name) const noexcept
    {
        return m_required_extensions.contains(extension_name);
    }
    ResourceLeaseList enableExtensions(vk::Instance instance) const noexcept
    {
        ResourceLeaseList ext_leases;
        for (const auto & callback : m_ext_enable_callbacks)
        {
            ext_leases.emplace_back(callback(instance));
        }
        return ext_leases;
    }
    std::size_t getRequiredExtensionCount() const noexcept { return m_required_extensions.size(); }
    void printUnsupportedExtensions() const noexcept;
private:
    StringSet m_required_extensions;
    ExtEnableCallbackList m_ext_enable_callbacks;
};

} // namespace lcf::vkc

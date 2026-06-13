#include "vk_core/details/instance_extensions/instance_extensions.h"
#include "vk_core/details/instance_extensions/debug_utils.h"

namespace lcf::vkc::details {

std::vector<ResourceLease> enable_instance_extensions(vk::Instance instance) noexcept
{
    std::vector<ResourceLease> extension_leases;
    if (auto lease = enable_debug_utils(instance)) {
        extension_leases.emplace_back(std::move(lease));
    }
    return extension_leases;
}

} // namespace lcf::vkc::details
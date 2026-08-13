#include "vk_core/context/InstanceContext.h"
#include "vk_core/bootstrap/info_structs.h"
#include "vk_core/bootstrap/create_instance.h"

namespace lcf::vkc {

Error InstanceContext::create(const bs::InstanceCreateInfo &instance_info, Error * warning_out) noexcept
{
    auto expected_instance = bs::create_instance(instance_info, warning_out);
    if (not expected_instance) { return expected_instance.error(); }
    m_instance = std::move(expected_instance.value());
    auto ext_enable_callback = instance_info.getExtensionEnableCallback();
    m_ext_resource_leases = ext_enable_callback(m_instance.get());
    return {};
}

} // namespace lcf::vkc
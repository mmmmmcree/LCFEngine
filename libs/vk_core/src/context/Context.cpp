#include "vk_core/context/Context.h"
#include "vk_core/context/DeviceContext.h"
#include "vk_core/context/QueueContext.h"
#include "vk_core/context/create_infos.h"
#include "vk_core/bootstrap/create_instance.h"
#include "vk_core/error.h"
#include "enums/enum_values.h"

namespace lcf::vkc {

Context::~Context() noexcept = default;

std::error_code Context::create(const ContextCreateInfo &create_info) noexcept
{
    auto expected_instance = bs::create_instance(create_info.getInstanceCreateInfo());
    if (not expected_instance) { return expected_instance.error(); }
    m_instance = std::move(expected_instance.value());
    auto ext_enable_callback = create_info.getInstanceCreateInfo().getExtensionEnableCallback();
    m_ext_resource_leases = ext_enable_callback(m_instance.get());
    for (auto device_role : enum_values_v<DeviceRole>) {
        const auto & device_context_create_info_opt = create_info.getDeviceContextCreateInfo(device_role);
        if (not device_context_create_info_opt) {
            m_device_context_table[std::to_underlying(device_role)] = m_device_context_table[std::to_underlying(DeviceRole::eMain)];
            continue;
        }
        const auto & device_context_create_info = device_context_create_info_opt.value();
        auto device_context_up = std::make_unique<DeviceContext>();
        if (auto ec = device_context_up->create(m_instance.get(), device_context_create_info)) { return ec; }
        m_device_context_table[std::to_underlying(device_role)] = device_context_up.get();
        m_device_contexts.emplace_back(std::move(device_context_up));
    }
    return {};
}

} // namespace lcf::vkc


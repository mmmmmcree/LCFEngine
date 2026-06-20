#include "vk_core/context/DeviceContext.h"
#include "vk_core/context/QueueContext.h"
#include "vk_core/context/create_infos.h"
#include "vk_core/bootstrap/select_physical_device.h"
#include "vk_core/bootstrap/create_device.h"

namespace stdr = std::ranges;
namespace stdv = std::views;

namespace {

using namespace lcf::vkc;

using QueueFamilyRequest = std::pair<uint32_t, uint32_t>;   // {family_index, queue_count}

using QueueFamilyRequestList = std::vector<QueueFamilyRequest>;

using QueueFamilyPropertiesSpan = std::span<const vk::QueueFamilyProperties>;

using QueueFamilyRequestFlagsPair = std::pair<vk::QueueFlags, vk::QueueFlags>; // {desired_flags, undesired_flags}

std::optional<uint32_t> find_proper_queue_family_index(
    QueueFamilyPropertiesSpan properties,
    vk::QueueFlags desired_flags,
    vk::QueueFlags undesired_flags) noexcept;

QueueFamilyRequestList gen_queue_family_requests(vk::PhysicalDevice physical_device, DeviceRole device_role) noexcept;

QueueFamilyRequestList gen_queue_family_requests_for_main_device(QueueFamilyPropertiesSpan properties) noexcept;

} // anonymous namespace

namespace lcf::vkc {

DeviceContext::~DeviceContext() noexcept = default;

std::error_code DeviceContext::create(vk::Instance instance, const DeviceContextCreateInfo &create_info) noexcept
{
    DeviceContextCreateInfo device_context_info = create_info;
    auto expected_physical_device = bs::select_physical_device(instance, device_context_info.getPhysicalDeviceSelectInfo());
    if (not expected_physical_device) { return expected_physical_device.error(); }
    m_physical_device = expected_physical_device.value();
    auto queue_family_requests = gen_queue_family_requests(m_physical_device, device_context_info.getDeviceRole());   
    device_context_info.addQueueFamilyRequests(queue_family_requests);
    auto expected_device = bs::create_device(m_physical_device, create_info.getDeviceCreateInfo());
    if (not expected_device) { return expected_device.error(); }
    m_device = std::move(expected_device.value());
    for (const auto & [family_index, queue_count] : queue_family_requests) {

    }
    return {};
}

} // namespace lcf::vkc

namespace {

std::optional<uint32_t> find_proper_queue_family_index(
    QueueFamilyPropertiesSpan properties,
    vk::QueueFlags desired_flags,
    vk::QueueFlags undesired_flags) noexcept
{
    std::optional<uint32_t> found_family_index = std::nullopt;
    for (const auto & [family_index, props] : properties | stdv::enumerate) {
        if (not (props.queueFlags & desired_flags)) { continue; }
        found_family_index = static_cast<uint32_t>(family_index);
        if (not (props.queueFlags & undesired_flags)) { break; }
    }
    return found_family_index;
}


QueueFamilyRequestList gen_queue_family_requests(vk::PhysicalDevice physical_device, DeviceRole device_role) noexcept
{
    auto queue_family_properties = physical_device.getQueueFamilyProperties();   
    switch (device_role) {
        case DeviceRole::eMain: { return gen_queue_family_requests_for_main_device(queue_family_properties); }
        case DeviceRole::eCompute:
        default: { return {}; }
    }
    return {};
}

QueueFamilyRequestList gen_queue_family_requests_for_main_device(QueueFamilyPropertiesSpan properties) noexcept
{
    static constexpr QueueFamilyRequestFlagsPair s_desired_flags [] = {
        { vk::QueueFlagBits::eGraphics, {} },
        { vk::QueueFlagBits::eCompute, vk::QueueFlagBits::eTransfer },
        { vk::QueueFlagBits::eTransfer, vk::QueueFlagBits::eCompute },
    };
    QueueFamilyRequestList requests;
    for (const auto & [desired_flags, undesired_flags] : s_desired_flags) {
        auto queue_family_index = find_proper_queue_family_index(properties, desired_flags, undesired_flags);
        if (queue_family_index) {
            requests.emplace_back(*queue_family_index, 1); //- 1 queue per family
        }
    }
    return requests;
}

} // anonymous namespace
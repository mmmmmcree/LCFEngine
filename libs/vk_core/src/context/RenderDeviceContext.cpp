#include "vk_core/context/RenderDeviceContext.h"
#include "vk_core/context/QueueContext.h"
#include "vk_core/context/create_infos.h"
#include "vk_core/bootstrap/select_physical_device.h"
#include "vk_core/bootstrap/create_device.h"
#include "vk_core/utils/find_proper_queue_family_index.h"
#include "vk_core/error.h"

namespace stdr = std::ranges;
namespace stdv = std::views;

namespace {

using namespace lcf::vkc;

using QueueFamilyRequestFlagsPair = std::pair<vk::QueueFlags, vk::QueueFlags>; // {desired_flags, undesired_flags}

using QueueFamilyMap = std::unordered_map<uint32_t, vk::QueueFlags>; // {family_index, desired_flags}

QueueFamilyMap find_queue_families(
    std::span<const vk::QueueFamilyProperties> properties,
    std::span<const QueueFamilyRequestFlagsPair> required_flags_pairs) noexcept;

} // anonymous namespace

namespace lcf::vkc {

RenderDeviceContext::~RenderDeviceContext() noexcept = default;

std::error_code RenderDeviceContext::create(vk::Instance instance, const DeviceContextCreateInfo &create_info) noexcept
{
    static constexpr QueueFamilyRequestFlagsPair s_required_flags_pairs [] = {
        { vk::QueueFlagBits::eGraphics, {} },
        { vk::QueueFlagBits::eCompute, vk::QueueFlagBits::eTransfer },
        { vk::QueueFlagBits::eTransfer, vk::QueueFlagBits::eCompute },
    };
    auto expected_physical_device = bs::select_physical_device(instance, create_info.getPhysicalDeviceSelectInfo());
    if (not expected_physical_device) { return expected_physical_device.error(); }
    m_physical_device = expected_physical_device.value();
    auto queue_family_properties = m_physical_device.getQueueFamilyProperties();   
    auto queue_family_map = find_queue_families(queue_family_properties, s_required_flags_pairs);   
    if (queue_family_map.empty()) { return errc::no_suitable_queue_family; }
    bs::DeviceCreateInfo device_info = create_info.getDeviceCreateInfo();
    device_info.addQueueFamilyRequests(queue_family_map | stdv::keys |
        stdv::transform([](uint32_t family_index) { return std::make_pair(family_index, 1); }));
    auto expected_device = bs::create_device(m_physical_device, device_info);
    if (not expected_device) { return expected_device.error(); }
    m_device = std::move(expected_device.value());
    for (const auto & [family_index, desired_flags] : queue_family_map) {
        auto queue_context_up = std::make_unique<QueueContext>();
        queue_context_up->create(m_device.get(), family_index, 0); //- queue count is always 1, so index is always 0;
        if (desired_flags & vk::QueueFlagBits::eGraphics) {
            m_graphics_queue_context_p = queue_context_up.get();
        } else if (desired_flags & vk::QueueFlagBits::eCompute) {
            m_compute_queue_context_p = queue_context_up.get();
        } else if (desired_flags & vk::QueueFlagBits::eTransfer) {
            m_transfer_queue_context_p = queue_context_up.get();
        }
        m_queue_contexts.emplace_back(std::move(queue_context_up));
    }
    return {};
}

} // namespace lcf::vkc

namespace {

QueueFamilyMap find_queue_families(
    std::span<const vk::QueueFamilyProperties> properties ,
    std::span<const QueueFamilyRequestFlagsPair> required_flags_pairs) noexcept
{
    QueueFamilyMap requests;
    for (const auto & [desired_flags, undesired_flags] : required_flags_pairs) {
        auto queue_family_index = utils::find_proper_queue_family_index(properties, desired_flags, undesired_flags);
        if (queue_family_index) {
            requests[*queue_family_index] |= desired_flags;
        }
    }
    return requests;
}

} // anonymous namespace
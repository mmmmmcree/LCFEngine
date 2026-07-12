#include "vk_core/context/DeviceContext.h"
#include "vk_core/context/QueueContext.h"
#include "vk_core/queue/details/DeviceQueue.h"
#include "vk_core/memory/info_structs.h"
#include "vk_core/memory/MemoryAllocator.h"
#include "vk_core/context/info_structs.h"
#include "vk_core/bootstrap/select_physical_device.h"
#include "vk_core/bootstrap/create_device.h"
#include "vk_core/utils/find_proper_queue_family_index.h"
#include "vk_core/utils/physical_device_feature_utils.h"
#include "vk_core/error.h"

namespace stdr = std::ranges;
namespace stdv = std::views;

namespace {

using namespace lcf::vkc;
using namespace lcf::vkc::details;

struct DeviceQueueInfo
{
    uint32_t family_index = 0;
    uint32_t queue_index = 0;
    std::vector<uint32_t> request_indices;
    DeviceQueue::SharingMode sharing_mode = {};
};

std::vector<DeviceQueueInfo> solve_queue_requests(vk::PhysicalDevice physical_device, std::span<const QueueRequest> queue_requests) noexcept;

} // anonymous namespace

namespace lcf::vkc {

DeviceContext::~DeviceContext() noexcept = default;

std::error_code DeviceContext::create(vk::Instance instance, const DeviceContextCreateInfo &create_info) noexcept
{
    // static constexpr QueueFamilyRequestFlagsPair s_required_flags_pairs [] = {
    //     { vk::QueueFlagBits::eGraphics, {} },
    //     { vk::QueueFlagBits::eCompute, vk::QueueFlagBits::eTransfer },
    //     { vk::QueueFlagBits::eTransfer, vk::QueueFlagBits::eCompute },
    // };
    // auto expected_physical_device = bs::select_physical_device(instance, create_info.getPhysicalDeviceSelectInfo());
    // if (not expected_physical_device) { return expected_physical_device.error(); }
    // m_physical_device = expected_physical_device.value();
    // auto queue_family_properties = m_physical_device.getQueueFamilyProperties();   
    // auto queue_family_map = find_queue_families(queue_family_properties, s_required_flags_pairs);   
    // if (queue_family_map.empty()) { return errc::no_suitable_queue_family; }
    // bs::DeviceCreateInfo device_info = create_info.getDeviceCreateInfo();
    // device_info.addQueueFamilyRequests(queue_family_map | stdv::keys |
    //     stdv::transform([](uint32_t family_index) { return std::make_pair(family_index, 2); }));
    // auto expected_device = bs::create_device(m_physical_device, device_info);
    // if (not expected_device) { return expected_device.error(); }
    // m_device = std::move(expected_device.value());
    // for (const auto & [family_index, desired_flags] : queue_family_map) {
    //     auto queue_context_up = std::make_unique<QueueContext>();
    //     if (auto ec = queue_context_up->create(m_device.get(), family_index, 0)) { return ec; } //- queue count is always 1, so index is always 0;
    //     if (desired_flags & vk::QueueFlagBits::eGraphics) {
    //         m_graphics_queue_context_p = queue_context_up.get();
    //     } else if (desired_flags & vk::QueueFlagBits::eCompute) {
    //         m_compute_queue_context_p = queue_context_up.get();
    //     } else if (desired_flags & vk::QueueFlagBits::eTransfer) {
    //         m_transfer_queue_context_p = queue_context_up.get();
    //     }
    //     m_queue_contexts.emplace_back(std::move(queue_context_up));
    // }
    // MemoryAllocatorCreateInfo allocator_create_info;
    // allocator_create_info.setBufferDeviceAddress(device_info.isFeatureRequired(
    //     utils::t_feature_bit<&vk::PhysicalDeviceVulkan12Features::bufferDeviceAddress>));
    // if (auto ec = m_memory_allocator.create(instance, m_physical_device, m_device.get(), allocator_create_info)) { return ec; }
    return {};
}

} // namespace lcf::vkc

namespace {

std::vector<DeviceQueueInfo> solve_queue_requests(vk::PhysicalDevice physical_device, std::span<const QueueRequest> queue_requests) noexcept
{
    /*
    1. merge requests -> map from QueueRequest to std::vector<uint32_t>: request_indices
    2. find suitable queue families -> map from QueueRequest to std::vector<uint32_t>: family_indices, sort by order
        if any request can't be satisfied, return empty result;
    3. merge requests with same submission tag -> map from tag to QueueRequests
    4. 
        maintain a dirty map for <queue family index, queue index>
        maintain remain count for each queue family
        for each tag:
            for each QueueRequest:
                std::optional<DeviceQueueInfo> found_info;
                for each queue family: (read from step 2)
                    auto & remaining_count = remain_count[queue_family_index];
                    if (remaining_count == 0) { continue; }
                    queue_index = queue_family_props[queue_family_index].queueCount - remaining_count;
                    found_info.emplace(queue_family_index, queue_index, request_indices: read from step 1})
                    --remaining_count;
                if found_info is empty:
                    return empty result;

    */
    // auto queue_family_props = physical_device.getQueueFamilyProperties();   
    // auto remaining_queue_count = queue_family_props | stdv::transform(&vk::QueueFamilyProperties::queueCount);
    // vk::QueueFamilyProperties;
    // //1.
    // std::unordered_map<QueueRequest, std::vector<uint32_t>> merged_requests;
    // for (auto && [index, queue_request] : queue_requests | stdv::enumerate) {
    //     merged_requests[queue_request].emplace_back(index);
    // }
    // std::unordered_map<QueueSubmissionThreadTag, std::vector<QueueRequest>> tag_grouped_requests;
    // std::unordered_map<QueueRequest, std::vector<uint32_t>> suitable_queue_family_indices;
    // //2.
    // for (auto && [queue_request, _] : merged_requests) {
    //     std::vector<uint32_t> best_matched_family_indices;
    //     std::vector<uint32_t> matched_family_indices;
    //     for (const auto & [family_index, props] : queue_family_props | std::views::enumerate) {
    //         if (not (props.queueFlags & queue_request.required_flags)) { continue; }
    //         uint32_t found_family_index = static_cast<uint32_t>(family_index);
    //         if (not (props.queueFlags & queue_request.undesired_flags)) {
    //             best_matched_family_indices.emplace_back(found_family_index);
    //         } else {
    //             matched_family_indices.emplace_back(found_family_index);
    //         }
    //     }
    //     if (best_matched_family_indices.empty() and matched_family_indices.empty()) { return {}; }
    //     best_matched_family_indices.append_range(std::move(matched_family_indices));
    //     suitable_queue_family_indices[queue_request] = std::move(best_matched_family_indices);
    //     tag_grouped_requests[queue_request.submission_thread_tag].emplace_back(queue_request);
    // }
    // //3.
    // std::vector<DeviceQueueInfo> device_queue_infos;
    // for (auto && [tag, requests] : tag_grouped_requests) {
    //     for (auto && request : requests) {
    //         for (auto & family_index : suitable_queue_family_indices[request]) {
    //             auto & remaining_count = remaining_queue_count[family_index];
    //             if (remaining_count == 0) { continue; }
    //             uint32_t queue_index = queue_family_props[family_index].queueCount - remaining_count;
    //             device_queue_infos.emplace_back(DeviceQueueInfo {family_index, queue_index, std::move(merged_requests[request])});
    //         }
    //     }
    // }
    // return device_queue_infos;
    return {};
}

} // anonymous namespace
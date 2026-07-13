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
    using Self = DeviceQueueInfo;
    using RequestIndices = std::vector<uint32_t>;
    
    Self & setFamilyIndex(uint32_t family_index) { m_family_index = family_index; return *this; }
    Self & setQueueIndex(uint32_t queue_index) { m_queue_index = queue_index; return *this; }
    Self & setSharingMode(DeviceQueue::SharingMode sharing_mode) { m_sharing_mode = sharing_mode; return *this; }
    Self & setRequestIndices(RequestIndices request_indices) noexcept { m_request_indices = std::move(request_indices); return *this; }
    Self & setQueuePriority(float queue_priority) noexcept { m_queue_priority = queue_priority; return *this; }
    
    uint32_t m_family_index = 0;
    uint32_t m_queue_index = 0;
    DeviceQueue::SharingMode m_sharing_mode = {};
    float m_queue_priority = 1.0f;
    RequestIndices m_request_indices;
};

std::expected<std::vector<DeviceQueueInfo>, std::error_code> solve_queue_requests(
    vk::PhysicalDevice physical_device,
    std::span<const QueueRequest> queue_requests) noexcept;

} // anonymous namespace

namespace lcf::vkc {

DeviceContext::~DeviceContext() noexcept = default;

std::error_code DeviceContext::create(vk::Instance instance, const DeviceContextCreateInfo &create_info) noexcept
{
    auto expected_physical_device = bs::select_physical_device(instance, create_info.getPhysicalDeviceSelectInfo());
    if (not expected_physical_device) { return expected_physical_device.error(); }
    m_physical_device = expected_physical_device.value();
    const auto & queue_requests = create_info.getQueueRequests(); 
    auto expected_device_queue_infos = solve_queue_requests(m_physical_device, queue_requests);
    if (not expected_device_queue_infos) { return expected_device_queue_infos.error(); }
    bs::DeviceCreateInfo device_info = create_info.getDeviceCreateInfo();
    auto & device_queue_infos = expected_device_queue_infos.value();
    for (const auto & device_queue_info : device_queue_infos) {
        device_info.addQueueFamilyRequest({device_queue_info.m_family_index, device_queue_info.m_queue_priority});
    }
    auto expected_device = bs::create_device(m_physical_device, device_info);
    if (not expected_device) { return expected_device.error(); }
    m_device = std::move(expected_device.value());
    m_device_queues.reserve(device_queue_infos.size());
    m_logical_queues.resize(queue_requests.size());
    for (const auto & device_queue_info : device_queue_infos) {
        auto device_queue_up = std::make_unique<details::DeviceQueue>(
            m_device.get(),
            device_queue_info.m_family_index,
            device_queue_info.m_queue_index,
            device_queue_info.m_sharing_mode);
        for (uint32_t req_id : device_queue_info.m_request_indices) {
            m_logical_queues[req_id] = LogicalQueue {*device_queue_up};
        }
        m_device_queues.emplace_back(std::move(device_queue_up));
    }
    MemoryAllocatorCreateInfo allocator_create_info;
    allocator_create_info.setBufferDeviceAddress(device_info.isFeatureRequired(
        utils::t_feature_bit<&vk::PhysicalDeviceVulkan12Features::bufferDeviceAddress>));
    if (auto ec = m_memory_allocator.create(instance, m_physical_device, m_device.get(), allocator_create_info)) { return ec; }
    return {};
}

} // namespace lcf::vkc

namespace {

struct QueueRequestSignature
{
    QueueRequestSignature(const QueueRequest & request) noexcept :
        m_required_flags(request.getRequiredFlags()), m_undesired_flags(request.getUndesiredFlags()) {}
    bool operator==(const QueueRequestSignature &) const noexcept = default;
    struct hasher
    {
        static constexpr uint64_t hash(const QueueRequestSignature & signature) noexcept
        {
            using MaskType = vk::QueueFlags::MaskType;
            return static_cast<uint64_t>(MaskType(signature.m_required_flags)) << 32 | static_cast<uint64_t>(MaskType(signature.m_undesired_flags));
        }
    };
    vk::QueueFlags m_required_flags = {};
    vk::QueueFlags m_undesired_flags = {};
};


struct EvaluatedPlan
{
    using Self = EvaluatedPlan;
    using DeviceQueueInfoList = std::vector<DeviceQueueInfo>;
    struct Cost
    {
        auto operator<=>(const Cost &) const = default;
        Cost & operator+=(const Cost & other) noexcept
        {
            forced_share_count += other.forced_share_count;
            fit_fallback_count += other.fit_fallback_count;
            mixed_signature_count += other.mixed_signature_count;
            total_queue_count += other.total_queue_count;
            return *this;
        }
        uint32_t forced_share_count = 0;
        uint32_t fit_fallback_count = 0;
        uint32_t mixed_signature_count = 0;
        uint32_t total_queue_count = 0;
    };
    
    Self & addCost(const Cost & cost) noexcept { m_cost += cost; return *this; }
    Self & addDeviceQueueInfo(DeviceQueueInfo device_queue_info) noexcept { m_device_queue_infos.emplace_back(std::move(device_queue_info)); return *this; }
    Self & merge(Self other) noexcept
    {
        m_cost += other.m_cost;
        m_device_queue_infos.append_range(std::move(other.m_device_queue_infos));
        return *this;
    }
    const Cost & getCost() const noexcept { return m_cost; }
    bool isBetterThan(const Self & other) const noexcept { return m_cost < other.m_cost; }
    
    DeviceQueueInfoList m_device_queue_infos;
    Cost m_cost;
};

struct SubmissionThreadTaggedGroup
{
    using Self = SubmissionThreadTaggedGroup;
    using RequestIndices = std::vector<uint32_t>;
    using SignatureCluster = std::unordered_map<QueueRequestSignature, RequestIndices, QueueRequestSignature::hasher>;

    bool operator>(const Self & other) const noexcept
    {
        if (m_max_priority != other.m_max_priority) { return m_max_priority > other.m_max_priority; }
        return m_controlled_request_count > other.m_controlled_request_count;
    }
    void mergeRequest(uint32_t id, const QueueRequest & request) noexcept
    {
        m_signature_cluster[QueueRequestSignature(request)].emplace_back(id);
        m_max_priority = std::max(m_max_priority, request.getPriority());
        ++m_controlled_request_count;
    }

    SignatureCluster m_signature_cluster;
    float m_max_priority = 0.0f;
    uint32_t m_controlled_request_count = 0;
};

struct QueueAssignment
{
    using Self = QueueAssignment;
    using RequestIndices = std::vector<uint32_t>;
    using RequestIndicesList = std::vector<RequestIndices>;
    QueueAssignment() noexcept = default;
    explicit QueueAssignment(SubmissionThreadTaggedGroup && tagged_group) noexcept :
        m_clustered_requests_list(tagged_group.m_signature_cluster | stdv::values | stdv::as_rvalue | stdr::to<std::vector>()),
        m_max_priority(tagged_group.m_max_priority)
    {
    }
    void absorb(Self other) noexcept
    {
        m_clustered_requests_list.append_range(std::move(other.m_clustered_requests_list));
        m_tag_count += other.m_tag_count;
        m_max_priority = std::max(m_max_priority, other.m_max_priority);
    }
    Self split() noexcept
    {
        Self splitted;
        splitted.m_clustered_requests_list.emplace_back(std::move(m_clustered_requests_list.back()));
        m_clustered_requests_list.pop_back();
        return splitted;
    }
    uint32_t getSignatureCount() const noexcept { return static_cast<uint32_t>(m_clustered_requests_list.size()); }
    uint32_t getMixedSignatureCount() const noexcept { return this->getSignatureCount() - m_tag_count; }
    bool hasMixedSignatures() const noexcept { return this->getMixedSignatureCount(); }
    uint32_t getForcedShareCount() const noexcept { return m_tag_count - 1; }
    DeviceQueue::SharingMode getSharingMode() const noexcept
    {
        return m_tag_count > 1 ?  DeviceQueue::SharingMode::Shared : DeviceQueue::SharingMode::Exclusive;
    }

    RequestIndicesList m_clustered_requests_list;
    uint32_t m_tag_count = 1;
    float m_max_priority = 0.0f;   
};

class QueueRequestSolver
{
    using DeviceQueueInfoList = std::vector<DeviceQueueInfo>;
    using CandidateFamilies = std::vector<uint32_t>;
    using RequestIndices = std::vector<uint32_t>;
public:
    std::expected<DeviceQueueInfoList, std::error_code> solve(vk::PhysicalDevice physical_device, std::span<const QueueRequest> requests) noexcept
    {
        m_requests = requests;
        m_family_of_requests.assign(requests.size(), 0u);
        if (auto ec = this->collectCandidateFamilies(physical_device)) { return std::unexpected<std::error_code>(ec); }
        this->makeSearchOrder();
        this->dfs(0);
        if (not m_best_plan) { return std::unexpected<std::error_code>(errc::no_suitable_queue_family); }
        return std::move(m_best_plan->m_device_queue_infos);
    }
private:
    std::error_code collectCandidateFamilies(vk::PhysicalDevice physical_device) noexcept
    {
        try {
            m_family_props_list = physical_device.getQueueFamilyProperties();
        } catch (const vk::SystemError &e) { return e.code(); }
        m_candidate_families_list.resize(m_requests.size());
        for (auto && [request, candidate_families] : stdv::zip(m_requests, m_candidate_families_list)) {
            for (auto && [family_index, family_props] : m_family_props_list | stdv::enumerate) {
                bool flags_satisfied = (family_props.queueFlags & request.getRequiredFlags()) == request.getRequiredFlags();
                if (not flags_satisfied) { continue; }
                vk::SurfaceKHR present_surface = request.getPresentSurface();
                bool present_supported = false;
                try {
                    present_supported = present_surface ? physical_device.getSurfaceSupportKHR(family_index, present_surface) : true;
                } catch (const vk::SystemError &e) { return e.code(); }
                if (not present_supported) { continue; }
                candidate_families.emplace_back(family_index);
            }
            if (candidate_families.empty()) { return errc::no_suitable_queue_family; }
        }
        return {};
    }
    void makeSearchOrder()
    {
        m_search_order = stdv::iota(0u, static_cast<uint32_t>(m_requests.size())) | stdr::to<std::vector>();
        stdr::sort(m_search_order, [this](uint32_t lhs_idx, uint32_t rhs_idx) {
            std::size_t lhs_candidates_families_size = m_candidate_families_list[lhs_idx].size();
            std::size_t rhs_candidates_families_size = m_candidate_families_list[rhs_idx].size();
            if (lhs_candidates_families_size != rhs_candidates_families_size) { return lhs_candidates_families_size < rhs_candidates_families_size; }
            float lhs_priority = m_requests[lhs_idx].getPriority();
            float rhs_priority = m_requests[rhs_idx].getPriority();
            if (lhs_priority != rhs_priority) { return lhs_priority > rhs_priority; }
            return lhs_idx < rhs_idx;
        });
    }
    void dfs(uint64_t depth)
    {
        if (depth == m_search_order.size()) {
            auto evaluated_plan = this->evaluateCurrentFamilyOfRequests();
            if (not m_best_plan or evaluated_plan.isBetterThan(m_best_plan.value())) {
                m_best_plan.emplace(std::move(evaluated_plan));
            }
            return;
        }
        uint32_t req_id = m_search_order[depth];
        for (uint32_t family_index : m_candidate_families_list[req_id]) {
            m_family_of_requests[req_id] = family_index;
            this->dfs(depth + 1);
        }
    }
    EvaluatedPlan evaluatePerFamily(uint32_t family_index, std::span<const uint32_t> req_ids) const
    {
        uint32_t available_queue_count = m_family_props_list[family_index].queueCount;
        std::vector<SubmissionThreadTaggedGroup> tagged_groups;
        std::unordered_map<QueueSubmissionThreadTag, uint32_t> tagged_group_id_map;
        for (uint32_t id : req_ids) {
            const QueueRequest & request = m_requests[id];
            auto [it, inserted] = tagged_group_id_map.try_emplace(request.getSubmissionThreadTag(), static_cast<uint32_t>(tagged_groups.size()));
            if (inserted) { tagged_groups.emplace_back(); }
            tagged_groups[it->second].mergeRequest(id, request);
        }
        stdr::sort(tagged_groups, std::greater {});
        std::vector<QueueAssignment> queue_assignments;
        for (auto & tagged_group : tagged_groups) {
            if (queue_assignments.size() < available_queue_count) {
                queue_assignments.emplace_back(std::move(tagged_group));
            } else {
                auto & min_priority_assignment = *stdr::min_element(queue_assignments, {}, &QueueAssignment::m_max_priority);
                min_priority_assignment.absorb(QueueAssignment {std::move(tagged_group)});
            }
        }
        while (queue_assignments.size() < available_queue_count) {
            auto & max_assignment = *stdr::max_element(queue_assignments, {}, &QueueAssignment::getSignatureCount);
            if (not max_assignment.hasMixedSignatures()) { break; }
            auto split_result = max_assignment.split();   // split before emplace_back to avoid invalidation
            queue_assignments.emplace_back(std::move(split_result));
        }
        EvaluatedPlan evaluated_plan;
        for (uint32_t queue_index = 0; queue_index < queue_assignments.size(); ++queue_index) {
            auto & assignment = queue_assignments[queue_index];
            EvaluatedPlan::Cost cost;
            cost.forced_share_count += assignment.getForcedShareCount();
            cost.mixed_signature_count += assignment.getMixedSignatureCount();
            float max_priority = 0.0f;
            for (const auto & cluster : assignment.m_clustered_requests_list) {
                for (uint32_t req_id : cluster) { max_priority = std::max(max_priority, m_requests[req_id].getPriority()); }
            }
            DeviceQueueInfo device_queue_info;
            device_queue_info.setFamilyIndex(family_index)
                .setQueueIndex(queue_index)
                .setSharingMode(assignment.getSharingMode())
                .setQueuePriority(max_priority)
                .setRequestIndices(assignment.m_clustered_requests_list | stdv::join | stdr::to<std::vector>());
            evaluated_plan.addCost(cost);
            evaluated_plan.addDeviceQueueInfo(std::move(device_queue_info));
        }
        return evaluated_plan;
    }
    EvaluatedPlan evaluateCurrentFamilyOfRequests() const
    {
        EvaluatedPlan evaluated_plan;
        std::vector<RequestIndices> family_request_ids(m_family_props_list.size());
        for (uint32_t req_id = 0; req_id < m_requests.size(); ++req_id) {
            family_request_ids[m_family_of_requests[req_id]].emplace_back(req_id);
        }
        for (uint32_t family_index = 0; family_index < m_family_props_list.size(); ++family_index) {
            auto & req_ids = family_request_ids[family_index];
            if (req_ids.empty()) { continue; }
            evaluated_plan.merge(this->evaluatePerFamily(family_index, req_ids));
        }
        for (const auto & [request, family_index] : stdv::zip(m_requests, m_family_of_requests)) {
            bool contains_undesired = bool(m_family_props_list[family_index].queueFlags & request.getUndesiredFlags());
            evaluated_plan.m_cost.fit_fallback_count += contains_undesired;
        }
        return evaluated_plan;
    }
private: 
    vk::PhysicalDevice m_physical_device;
    std::vector<vk::QueueFamilyProperties> m_family_props_list;    
    std::span<const QueueRequest> m_requests;
    std::vector<CandidateFamilies> m_candidate_families_list;
    std::vector<uint32_t> m_search_order;
    std::vector<uint32_t> m_family_of_requests;
    std::optional<EvaluatedPlan> m_best_plan;
};

std::expected<std::vector<DeviceQueueInfo>, std::error_code> solve_queue_requests(
    vk::PhysicalDevice physical_device,
    std::span<const QueueRequest> queue_requests) noexcept
{
    QueueRequestSolver solver;
    return solver.solve(physical_device, queue_requests);
}

} // anonymous namespace
#include "vk_core/descriptor_set/pool/DescriptorSetAllocator.h"
#include "vk_core/descriptor_set/pool/DescriptorSetLayout.h"
#include <ranges>
#include <algorithm>
#include <utility>

namespace stdr = std::ranges;
namespace stdv = std::views;

namespace {

using namespace lcf::vkc::dsp;
using namespace lcf::vkc::dsp::details;

PoolCapacityMap make_required_capacity(const DescriptorSetLayout & layout) noexcept;

VkDescriptorPoolCreateFlags deduce_pool_group_key(const DescriptorSetLayout & layout) noexcept;

} // anonymous namespace

namespace lcf::vkc::dsp {

bool DescriptorSetAllocator::PoolSetCompare::operator()(vk::DescriptorPool lhs, vk::DescriptorPool rhs) const noexcept
{
    if (lhs == rhs) { return false; }
    const auto lhs_priority = m_pool_states_p->find(lhs)->second.getPriority();
    const auto rhs_priority = m_pool_states_p->find(rhs)->second.getPriority();
    if (lhs_priority != rhs_priority) { return lhs_priority > rhs_priority; }
    return lhs < rhs;
}

DescriptorSetAllocator::~DescriptorSetAllocator() noexcept
{
    for (const auto &[pool, _] : m_pool_states) { m_device.destroyDescriptorPool(pool); }
}

DescriptorSetAllocator::DescriptorSetAllocator(Self && other) noexcept :
    m_device(std::exchange(other.m_device, {})),
    m_allocator_info(std::move(other.m_allocator_info)),
    m_pool_states(std::move(other.m_pool_states)),
    m_pool_sets(std::move(other.m_pool_sets))
{
    for (auto && [_, pools] : m_pool_sets) {
        pools = PoolSet(std::sorted_unique, std::move(pools).extract(), PoolSetCompare {&m_pool_states});
    }
    other.m_pool_sets.clear();
    other.m_pool_states.clear();
}

DescriptorSetAllocator & DescriptorSetAllocator::operator=(Self && other) noexcept
{
    if (this == &other) { return *this; }
    for (const auto &[pool, _] : m_pool_states) { m_device.destroyDescriptorPool(pool); }
    m_pool_sets.clear();
    m_pool_states = std::move(other.m_pool_states);
    m_pool_sets = std::move(other.m_pool_sets);
    m_device = std::exchange(other.m_device, {});
    m_allocator_info = std::move(other.m_allocator_info);
    for (auto && [_, pools] : m_pool_sets) {
        pools = PoolSet(std::sorted_unique, std::move(pools).extract(), PoolSetCompare {&m_pool_states});
    }
    other.m_pool_sets.clear();
    other.m_pool_states.clear();
    return *this;
}

std::error_code DescriptorSetAllocator::create(vk::Device device, const DescriptorSetAllocatorInfo & info) noexcept
{
    if (not device or info.getMaxSetsPerPool() == 0u) { return std::make_error_code(std::errc::invalid_argument); }
    for (const auto &[pool, _] : m_pool_states) { m_device.destroyDescriptorPool(pool); }
    m_pool_sets.clear();
    m_pool_states.clear();
    m_device = device;
    m_allocator_info = info;
    return {};
}

std::expected<DescriptorSetAllocation, std::error_code> DescriptorSetAllocator::allocate(const DescriptorSetLayout & layout, uint32_t count) noexcept
{
    const auto required_capacity = make_required_capacity(layout);
    const auto pool_group_key = deduce_pool_group_key(layout);
    auto pool_set_it = m_pool_sets.find(pool_group_key);
    if (pool_set_it == m_pool_sets.end()) {
        pool_set_it = m_pool_sets.emplace(pool_group_key, PoolSet(PoolSetCompare {&m_pool_states})).first;
    }
    auto & pool_set = pool_set_it->second;

    std::vector<vk::DescriptorSetLayout> layouts(count, layout.handle());
    for (const auto pool : pool_set) {
        auto & state = m_pool_states.find(pool)->second;
        if (not state.fits(count, required_capacity)) { continue; }
        auto descriptor_sets_result = this->allocateFromPool(pool, layouts);
        if (not descriptor_sets_result) {
            const auto error = descriptor_sets_result.error();
            if (error == vk::Result::eErrorOutOfPoolMemory or error == vk::Result::eErrorFragmentedPool) { continue; }
            return std::unexpected(error);
        }
        pool_set.erase(pool);
        state.consume(count, required_capacity);
        pool_set.emplace(pool);
        return DescriptorSetAllocation {pool, pool_group_key, std::move(*descriptor_sets_result)};
    }

    PoolState state;
    auto pool_result = this->createPool(pool_group_key, count, required_capacity, state);
    if (not pool_result) { return std::unexpected(pool_result.error()); }
    const auto pool = *pool_result;
    auto descriptor_sets_result = this->allocateFromPool(pool, layouts);
    if (not descriptor_sets_result) { return std::unexpected(descriptor_sets_result.error()); }
    state.consume(count, required_capacity);
    m_pool_states.emplace(pool, std::move(state));
    pool_set.emplace(pool);
    return DescriptorSetAllocation {pool, pool_group_key, std::move(*descriptor_sets_result)};
}

void DescriptorSetAllocator::recycle(const DescriptorSetAllocation &allocation, ResourceLease lease) noexcept
{
    m_pending_allocations.emplace_back(allocation, std::move(lease));
    while (not m_pending_allocations.empty()) {
        auto && [allocation, lease] = m_pending_allocations.front();
        if (lease.getRefCount() > 1u) { break; }
        this->deallocate(std::move(allocation));
        m_pending_allocations.pop_front();
    }
}

void DescriptorSetAllocator::deallocate(DescriptorSetAllocation allocation) noexcept
{
    auto && [pool, pool_key, descriptor_sets] = allocation;
    auto state_it = m_pool_states.find(pool);
    auto pool_set_it = m_pool_sets.find(pool_key);
    auto & state = state_it->second;
    auto & pool_set = pool_set_it->second;
    pool_set.erase(pool);
    state.markDeallocate(static_cast<uint32_t>(descriptor_sets.size()));
    if (not state.isDestroyable()) {
        pool_set.emplace(pool);
        return;
    }
    m_device.destroyDescriptorPool(pool);
    m_pool_states.erase(state_it);
    if (pool_set.empty()) { m_pool_sets.erase(pool_set_it); }
}

std::expected<std::vector<vk::DescriptorSet>, std::error_code> DescriptorSetAllocator::allocateFromPool(
    vk::DescriptorPool pool,
    std::span<const vk::DescriptorSetLayout> layouts) noexcept
{
    vk::DescriptorSetAllocateInfo allocate_info;
    allocate_info.setDescriptorPool(pool).setSetLayouts(layouts);
    try {
        return m_device.allocateDescriptorSets(allocate_info);
    } catch (const vk::SystemError & error) {
        return std::unexpected(error.code());
    }
    return {};
}

std::expected<vk::DescriptorPool, std::error_code> DescriptorSetAllocator::createPool(
    VkDescriptorPoolCreateFlags pool_key,
    uint32_t set_count,
    const CapacityMap & required_capacity,
    PoolState & pool_state) noexcept
{
    CapacityMap pool_capacity = m_allocator_info.getPoolSizes();
    for (const auto &[descriptor_type, descriptor_count] : required_capacity) {
        const uint32_t required_descriptor_count = descriptor_count * set_count;
        auto [it, inserted] = pool_capacity.try_emplace(descriptor_type, required_descriptor_count);
        if (not inserted) { it->second = std::max(it->second, required_descriptor_count); }
    }
    auto pool_sizes = pool_capacity | stdv::filter([](const auto & item) noexcept { return item.second != 0u; }) |
        stdv::transform([](const auto & item) noexcept { return vk::DescriptorPoolSize {item.first, item.second}; }) |
        stdr::to<std::vector>();
    const uint32_t max_set_count = std::max(m_allocator_info.getMaxSetsPerPool(), set_count);
    vk::DescriptorPoolCreateInfo pool_info;
    pool_info.setFlags(vk::DescriptorPoolCreateFlags(pool_key))
        .setMaxSets(max_set_count)
        .setPoolSizes(pool_sizes);
    vk::DescriptorPool pool;
    try {
        pool = m_device.createDescriptorPool(pool_info);
        pool_state.create(max_set_count, std::move(pool_capacity));
    } catch (const vk::SystemError & error) {
        return std::unexpected(error.code());
    }
    return pool;
}

} // namespace lcf::vkc::dsp

namespace {

PoolCapacityMap make_required_capacity(const DescriptorSetLayout & layout) noexcept
{
    details::PoolCapacityMap required_capacity;
    for (const auto & binding : layout.getBindings()) {
        required_capacity[binding.descriptorType] += binding.descriptorCount;
    }
    return required_capacity;
}

VkDescriptorPoolCreateFlags deduce_pool_group_key(const DescriptorSetLayout & layout) noexcept
{
    vk::DescriptorPoolCreateFlags flags {};
    if (layout.getBindingFlags() & vk::DescriptorBindingFlagBits::eUpdateAfterBind) {
        flags |= vk::DescriptorPoolCreateFlagBits::eUpdateAfterBind;
    }
    return VkDescriptorPoolCreateFlags(flags);
}

} // anonymous namespace

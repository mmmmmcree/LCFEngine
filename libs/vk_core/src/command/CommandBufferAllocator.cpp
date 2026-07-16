#include "vk_core/command/CommandBufferAllocator.h"
#include "vk_core/command/info_structs.h"
#include "vk_core/command/CommandBufferProxy.h"

namespace lcf::vkc {

namespace {

template <typename PoolMap>
std::expected<std::vector<vk::CommandBuffer>, std::error_code> acquire_pool(
    PoolMap & pool_map,
    vk::Device device,
    uint32_t family_index,
    const CommandBufferPoolKey & key,
    uint32_t count) noexcept
{
    auto it = pool_map.find(key);
    if (it != pool_map.end()) { return it->second.allocate(count); }
    typename PoolMap::mapped_type pool;
    vk::CommandPoolCreateInfo pool_info {key.m_pool_flags, family_index};
    if (auto ec = pool.create(device, pool_info, key.m_cmd_level)) { return std::unexpected(ec); }
    return pool_map.emplace(key, std::move(pool)).first->second.allocate(count);
}

} // namespace

std::error_code CommandBufferAllocator::create(vk::Device device, uint32_t family_index, ValidationData validation_data) noexcept
{
    m_device = device;
    m_family_index = family_index;
    m_validation_data = validation_data;
    return {};
}

std::expected<CommandBufferBatch, std::error_code> CommandBufferAllocator::allocate(const CommandBufferAllocateInfo & info) noexcept
{
    CommandBufferPoolKey key {info};
    uint32_t count = info.getCount();
    auto expected_cmd_buffers = (info.getPoolFlags() & CommandPoolFlagBits::eResetCommandBuffer)
        ? acquire_pool(m_resetable_pool_map, m_device, m_family_index, key, count)
        : acquire_pool(m_rotating_pool_map, m_device, m_family_index, key, count);
    if (not expected_cmd_buffers) { return std::unexpected(expected_cmd_buffers.error()); }
    return CommandBufferBatch {std::move(expected_cmd_buffers.value()), key, m_validation_data};
}

void CommandBufferAllocator::retire(uint64_t timestamp, CommandBufferBatch && batch) noexcept
{
    if (batch.m_validation_data != m_validation_data) { return; }
    if (batch.m_pool_key.m_pool_flags & CommandPoolFlagBits::eResetCommandBuffer) {
        auto it = m_resetable_pool_map.find(batch.m_pool_key);
        if (it == m_resetable_pool_map.end()) { return; }
        it->second.retire(timestamp, std::move(batch.m_cmd_buffers));
    } else {
        auto it = m_rotating_pool_map.find(batch.m_pool_key);
        if (it == m_rotating_pool_map.end()) { return; }
        it->second.retire(timestamp, std::move(batch.m_cmd_buffers));
    }
}

void CommandBufferAllocator::recycle(uint64_t completed_timestamp) noexcept
{
    for (auto & [_, pool] : m_resetable_pool_map) { pool.recycle(completed_timestamp); }
    for (auto & [_, pool] : m_rotating_pool_map) { pool.recycle(completed_timestamp); }
}

} // namespace lcf::vkc

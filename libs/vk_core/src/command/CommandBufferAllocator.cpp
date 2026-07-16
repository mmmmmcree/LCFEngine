#include "vk_core/command/CommandBufferAllocator.h"
#include "vk_core/command/info_structs.h"
#include "vk_core/command/CommandBufferProxy.h"
#include <ranges>
#include <algorithm>

namespace stdr = std::ranges;
namespace stdv = std::views;

namespace lcf::vkc {

std::error_code CommandBufferAllocator::create(vk::Device device, uint32_t family_index, ValidationData validation_data) noexcept
{
    m_device = device;
    m_family_index = family_index;
    m_validation_data = validation_data;
    return {};
}

std::expected<CommandBufferBatch, std::error_code> CommandBufferAllocator::allocate(const CommandBufferAllocateInfo & info) noexcept
{
    CommandBufferList cmd_buffers;
    CommandBufferPoolKey key {info};
    if (info.getPoolFlags() & CommandPoolFlagBits::eResetCommandBuffer) {
        
    } else {

    }
    return CommandBufferBatch {std::move(cmd_buffers), key, m_validation_data};
}

void CommandBufferAllocator::retire(uint64_t timestamp, CommandBufferBatch && batch) noexcept
{
    if (batch.m_validation_data != m_validation_data) { return; }
    if (batch.m_pool_key.m_pool_flags & CommandPoolFlagBits::eResetCommandBuffer) {
        
    } else {

    }
}

void CommandBufferAllocator::recycle(uint64_t completed_timestamp) noexcept
{
    for (auto & [_, pool] : m_resetable_pool_map) { pool.recycle(completed_timestamp); }
    for (auto & [_, pool] : m_rotating_pool_map) { pool.recycle(completed_timestamp); }
}

} // namespace lcf::vkc

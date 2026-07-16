#pragma once

#include <vulkan/vulkan.hpp>
#include "enums.h"

namespace lcf::vkc {

class CommandBufferAllocateInfo
{
    using Self = CommandBufferAllocateInfo;
public:
    Self & addPoolFlags(CommandPoolFlags flags) noexcept { m_pool_flags |= flags; return *this; }
    Self & setLevel(vk::CommandBufferLevel level) noexcept { m_level = level; return *this; }
    Self & setCount(uint32_t count) noexcept { m_count = std::max(count, 1u); return *this; }
    CommandPoolFlags getPoolFlags() const noexcept  { return m_pool_flags; }
    vk::CommandBufferLevel getLevel() const noexcept { return m_level; }
    uint32_t getCount() const noexcept { return m_count; }
private:
    CommandPoolFlags m_pool_flags = {};
    vk::CommandBufferLevel m_level = vk::CommandBufferLevel::ePrimary;
    uint32_t m_count = 1u;
};

struct CommandBufferPoolKey
{
    CommandBufferPoolKey(
        vk::CommandPoolCreateFlags pool_flags = {},
        vk::CommandBufferLevel cmd_level = {}) noexcept :
        m_pool_flags(pool_flags),
        m_cmd_level(cmd_level) {}
    explicit CommandBufferPoolKey(const CommandBufferAllocateInfo & alloc_info) noexcept :
        m_pool_flags(alloc_info.getPoolFlags()), m_cmd_level(alloc_info.getLevel()) {}
    constexpr uint64_t operator()(const CommandBufferPoolKey & key) const noexcept
    {
        return static_cast<uint64_t>(static_cast<uint32_t>(key.m_pool_flags)) << 32 |
            static_cast<uint32_t>(key.m_cmd_level);
    }
    bool operator==(const CommandBufferPoolKey &) const noexcept = default;

    vk::CommandPoolCreateFlags m_pool_flags;
    vk::CommandBufferLevel m_cmd_level;
};

} // namespace lcf::vkc

#pragma once

#include <vulkan/vulkan.hpp>
#include <expected>
#include <unordered_map>
#include <system_error>
#include "vk_core/command/details/command_pools.h"
#include "vk_core/command/info_structs.h"

namespace lcf::vkc {

class CommandBufferBatch;

class CommandBufferAllocator
{
    using Self = CommandBufferAllocator;
    using CommandBufferList = std::vector<vk::CommandBuffer>;
    using ResetablePoolMap = std::unordered_map<CommandBufferPoolKey, details::ResetableCommandPool, CommandBufferPoolKey>;
    using RotatingPoolMap = std::unordered_map<CommandBufferPoolKey, details::RotatingCommandPool, CommandBufferPoolKey>;
    using ValidationData = const void *;
public:
    ~CommandBufferAllocator() noexcept = default;
    CommandBufferAllocator() = default;
    CommandBufferAllocator(const Self &) = delete;
    Self & operator=(const Self &) = delete;
    CommandBufferAllocator(Self &&) noexcept = default;
    Self & operator=(Self &&) noexcept = default;
public:
    std::error_code create(vk::Device device, uint32_t family_index, ValidationData validation_data) noexcept;
    std::expected<CommandBufferBatch, std::error_code> allocate(const CommandBufferAllocateInfo & info) noexcept;
    void retire(uint64_t timestamp, CommandBufferBatch && batch) noexcept;
    void recycle(uint64_t completed) noexcept;
private:
    vk::Device m_device;
    uint32_t m_family_index = 0u;
    ValidationData m_validation_data = nullptr;
    ResetablePoolMap m_resetable_pool_map;
    RotatingPoolMap m_rotating_pool_map;
};

} // namespace lcf::vkc

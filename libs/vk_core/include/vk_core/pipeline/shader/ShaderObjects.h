#pragma once

#include <vulkan/vulkan.hpp>
#include <array>
#include "resource_utils.h"

namespace lcf::vkc {

class CommandBufferProxy;

class ShaderObject;

class ShaderObjects
{
    using Self = ShaderObjects;
    using StageList = std::vector<vk::ShaderStageFlagBits>;
    using HandleList = std::vector<vk::ShaderEXT>;
    using LeaseList = std::vector<ResourceLease>;
public:
    ~ShaderObjects() noexcept = default;
    ShaderObjects() noexcept = default; 
    ShaderObjects(const Self &) noexcept = default;
    ShaderObjects(Self &&) noexcept = default;
    Self & operator=(const Self &) noexcept = default;
    Self & operator=(Self &&) noexcept = default;
public:
    Self & setStage(const ShaderObject & shader) noexcept;
    Self & unsetStages(vk::ShaderStageFlags stages) noexcept;
    Self & removeStage(vk::ShaderStageFlags stages) noexcept;
    void bind(CommandBufferProxy & cmd) const noexcept;
private:
    uint32_t findOrInsertSlot(vk::ShaderStageFlagBits stage) noexcept;
private:
    StageList m_stages;
    HandleList m_handles {};
    LeaseList m_leases;
    uint32_t m_slot_count = 0u;
};

} // namespace lcf::vkc

#pragma once

#include <vulkan/vulkan.hpp>
#include <expected>
#include <flat_map>
#include <span>
#include <system_error>
#include <utility>
#include "vk_core/pipeline/shader/enums.h"
#include "vk_core/utils/ResourceHandle.h"

namespace lcf::vkc {

class CommandBufferProxy;
class ShaderProgramInfo;
class ShaderObjectGroup;

class ShaderObject
{
    friend class ShaderObjectGroup;
    using Self = ShaderObject;
public:
    ~ShaderObject() noexcept = default;
    ShaderObject() noexcept = default;
    ShaderObject(const Self &) noexcept = default;
    ShaderObject(Self &&) noexcept = default;
    Self & operator=(const Self &) noexcept = default;
    Self & operator=(Self &&) noexcept = default;
    operator const vk::ShaderEXT &() const noexcept { return m_shader_rh.get(); }
public:
    std::error_code create(vk::Device device, const vk::ShaderCreateInfoEXT & info) noexcept;
    const vk::ShaderEXT & handle() const noexcept { return m_shader_rh.get(); }
    ResourceLease lease() const noexcept { return m_shader_rh.lease(); }
    const vk::ShaderStageFlagBits & getStage() const noexcept { return m_stage; }
    const vk::ShaderStageFlags & getNextStages() const noexcept { return m_next_stages; }
private:
    ShaderObject(
        vk::UniqueShaderEXT shader,
        vk::ShaderStageFlagBits stage,
        vk::ShaderStageFlags next_stages) noexcept :
        m_shader_rh(std::move(shader)),
        m_stage(stage),
        m_next_stages(next_stages) {}
private:
    utils::ResourceHandle<vk::ShaderEXT> m_shader_rh;
    vk::ShaderStageFlagBits m_stage = vk::ShaderStageFlagBits::eVertex;
    vk::ShaderStageFlags m_next_stages = {};
};

class ShaderObjectGroup
{
    using Self = ShaderObjectGroup;
    using ShaderObjectMap = std::flat_map<
        vk::ShaderStageFlagBits,
        ShaderObject,
        enum_traits<vk::ShaderStageFlagBits>::pipeline_order_less_t>;
public:
    ~ShaderObjectGroup() noexcept = default;
    ShaderObjectGroup() noexcept = default;
    ShaderObjectGroup(const Self &) noexcept = default;
    ShaderObjectGroup(Self &&) noexcept = default;
    Self & operator=(const Self &) noexcept = default;
    Self & operator=(Self &&) noexcept = default;
public:
    std::error_code create(
        vk::Device device,
        const ShaderProgramInfo & program_info,
        vk::ShaderCreateFlagsEXT flags = {}) noexcept;
    std::span<const ShaderObject> viewObjects() const noexcept { return m_objects.values(); }
private:
    ShaderObjectMap m_objects;
};

class ShaderObjectBindingState
{
    using Self = ShaderObjectBindingState;
    using StageHandleMap = std::flat_map<
        vk::ShaderStageFlagBits,
        vk::ShaderEXT,
        enum_traits<vk::ShaderStageFlagBits>::pipeline_order_less_t>;
    using StageLeaseMap = std::flat_map<
        vk::ShaderStageFlagBits,
        ResourceLease,
        enum_traits<vk::ShaderStageFlagBits>::pipeline_order_less_t>;
public:
    ~ShaderObjectBindingState() noexcept = default;
    ShaderObjectBindingState() noexcept = default;
    ShaderObjectBindingState(const Self &) noexcept = default;
    ShaderObjectBindingState(Self &&) noexcept = default;
    Self & operator=(const Self &) noexcept = default;
    Self & operator=(Self &&) noexcept = default;
public:
    Self & clear() noexcept;
    Self & setStage(const ShaderObject & shader) noexcept;
    Self & unsetStages(vk::ShaderStageFlags stages) noexcept;
    Self & removeStages(vk::ShaderStageFlags stages) noexcept;
    Self & removeStage(vk::ShaderStageFlags stages) noexcept { return removeStages(stages); }
    Self & assign(const ShaderObjectGroup & group) noexcept;
    Self & merge(const ShaderObjectGroup & group) noexcept;
    void bind(CommandBufferProxy & cmd) const noexcept;
private:
    StageHandleMap m_handles;
    StageLeaseMap m_leases;
};

} // namespace lcf::vkc

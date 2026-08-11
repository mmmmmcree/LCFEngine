#pragma once

#include <vulkan/vulkan.hpp>
#include <system_error>
#include <expected>
#include <span>
#include <vector>
#include "vk_core/utils/ResourceHandle.h"

namespace lcf::vkc {

class ShaderProgramInfo;

class ShaderObject
{
    friend class LinkedShaderObjectGroup;
    using Self = ShaderObject;
    using List = std::vector<ShaderObject>;
public:
    ~ShaderObject() noexcept = default;
    ShaderObject() noexcept = default;
    ShaderObject(const Self &) noexcept = default;
    ShaderObject(Self &&) noexcept = default;
    ShaderObject & operator=(const Self &) noexcept = default;
    ShaderObject & operator=(Self &&) noexcept = default;
    operator const vk::ShaderEXT &() const noexcept { return m_shader_rh.get(); }
public:
    static std::expected<List, std::error_code> createBatch(vk::Device device, const ShaderProgramInfo & program_info) noexcept;
public:
    std::error_code create(vk::Device device, const vk::ShaderCreateInfoEXT & info) noexcept;
    const vk::ShaderEXT & handle() const noexcept { return m_shader_rh.get(); }
    ResourceLease lease() const noexcept { return m_shader_rh.lease(); }
    const vk::ShaderStageFlagBits & getStage() const noexcept { return m_stage; }
private:
    ShaderObject(vk::UniqueShaderEXT shader, vk::ShaderStageFlagBits stage) noexcept :
        m_shader_rh(std::move(shader)),
        m_stage(stage) {}
private:
    utils::ResourceHandle<vk::ShaderEXT> m_shader_rh;
    vk::ShaderStageFlagBits m_stage = vk::ShaderStageFlagBits::eVertex;
};

class LinkedShaderObjectGroup
{
    using Self = LinkedShaderObjectGroup;
    using ShaderObjectList = std::vector<ShaderObject>;
public:
    ~LinkedShaderObjectGroup() noexcept = default;
    LinkedShaderObjectGroup() noexcept = default;
    LinkedShaderObjectGroup(const Self &) noexcept = default;
    LinkedShaderObjectGroup(Self &&) noexcept = default;
    Self & operator=(const Self &) noexcept = default;
    Self & operator=(Self &&) noexcept = default;
public:
    std::error_code create(vk::Device device, const ShaderProgramInfo & program_info) noexcept;
    std::span<const ShaderObject> viewObjects() const noexcept { return m_objects; }
private:
    ShaderObjectList m_objects;
};

class ShaderObjectGroup
{
    using Self = ShaderObjectGroup;
    using ShaderObjectMap = std::unordered_map<vk::ShaderStageFlagBits, ShaderObject>;
public:
    ~ShaderObjectGroup() noexcept = default;
    ShaderObjectGroup() noexcept = default;
    ShaderObjectGroup(const Self &) noexcept = default;
    ShaderObjectGroup(Self &&) noexcept = default;
    Self & operator=(const Self &) noexcept = default;
    Self & operator=(Self &&) noexcept = default;
public:
    std::error_code create(vk::Device device, const ShaderProgramInfo & program_info, vk::ShaderCreateFlagsEXT flags = {}) noexcept;
private:
    ShaderObjectMap m_objects;
};

} // namespace lcf::vkc

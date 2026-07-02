#pragma once

#include <vulkan/vulkan.hpp>
#include <span>
#include <vector>

namespace lcf::vkc {

class CommandBufferProxy;

class ShaderObject
{
    using Self = ShaderObject;
    using ShaderStageList = std::vector<vk::ShaderStageFlagBits>;
    using ShaderList = std::vector<vk::UniqueShaderEXT>;
    using ShaderHandleList = std::vector<vk::ShaderEXT>;
public:
    ~ShaderObject() noexcept = default;
    ShaderObject() noexcept = default;
    ShaderObject(const Self &) = delete;
    ShaderObject(Self &&) noexcept = default;
    ShaderObject & operator=(const Self &) = delete;
    ShaderObject & operator=(Self &&) noexcept = default;
public:
    std::error_code create(vk::Device device, std::span<const vk::ShaderCreateInfoEXT> shader_infos) noexcept;
    void bind(CommandBufferProxy & cmd) noexcept;
private:
    ShaderList m_shader_list;
    ShaderStageList m_bind_stages;
    ShaderHandleList m_bind_shaders;
};

} // namespace lcf::vkc
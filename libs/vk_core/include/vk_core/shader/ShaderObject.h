#pragma once

#include <vulkan/vulkan.hpp>

namespace lcf::vkc {

class ShaderObject
{
    using Self = ShaderObject;
public:
    ~ShaderObject() noexcept = default;
    ShaderObject() noexcept = default;
    ShaderObject(const Self &) = delete;
    ShaderObject(Self &&) noexcept = default;
    ShaderObject & operator=(const Self &) = delete;
    ShaderObject & operator=(Self &&) noexcept = default;
    operator vk::ShaderEXT() const noexcept { return m_shader.get(); }
public:
    std::error_code create(vk::Device device, const vk::ShaderCreateInfoEXT & info) noexcept;
    const vk::ShaderEXT & handle() const noexcept { return m_shader.get(); }
    const vk::ShaderStageFlagBits & getStage() const noexcept { return m_stage; }
private:
    vk::UniqueShaderEXT m_shader;
    vk::ShaderStageFlagBits m_stage;
};

} // namespace lcf::vkc
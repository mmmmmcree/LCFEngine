#pragma once

#include <vulkan/vulkan.hpp>
#include <system_error>

namespace lcf::vkc {

class ShaderModule
{
    using Self = ShaderModule;
public:
    ~ShaderModule() noexcept = default;
    ShaderModule() noexcept = default;
    ShaderModule(const Self &) = delete;
    ShaderModule(Self &&) noexcept = default;
    ShaderModule & operator=(const Self &) = delete;
    ShaderModule & operator=(Self &&) noexcept = default;
    operator vk::ShaderModule() const noexcept { return m_module.get(); }
public:
    std::error_code create(vk::Device device, const vk::ShaderModuleCreateInfo & info) noexcept;
    const vk::ShaderModule & handle() const noexcept { return m_module.get(); }
private:
    vk::UniqueShaderModule m_module;
};

} // namespace lcf::vkc

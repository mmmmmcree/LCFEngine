#include "vk_core/pipeline/shader/ShaderModule.h"

namespace lcf::vkc {

std::error_code ShaderModule::create(vk::Device device, const vk::ShaderModuleCreateInfo & info) noexcept
{
    try {
        m_module = device.createShaderModuleUnique(info);
    } catch (const vk::SystemError & e) {
        return e.code();
    }
    return {};
}

} // namespace lcf::vkc

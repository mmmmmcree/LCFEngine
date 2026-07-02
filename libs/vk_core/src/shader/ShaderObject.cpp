#include "vk_core/shader/ShaderObject.h"
#include "vk_core/shader/entry.h"
#include "vk_core/manifest/DeviceExtensionManifest.h"
#include "vk_core/command/CommandBufferProxy.h"
#include <array>

namespace lcf::vkc::entry {

void register_shader_object(DeviceExtensionManifest & manifest) noexcept
{
    static constexpr std::array k_extensions { vk::EXTShaderObjectExtensionName };
    static constexpr std::array k_features
    {
        LCF_VKC_UTILS_FEATURE_BIT(&vk::PhysicalDeviceShaderObjectFeaturesEXT::shaderObject),
    };
    manifest.addRequiredExtensions(k_extensions)
        .addRequiredFeatures(k_features);
}

} // namespace lcf::vkc::entry

namespace lcf::vkc {


void ShaderObject::bind(CommandBufferProxy & cmd)
{
    cmd.bindShadersEXT(m_bind_stages, m_bind_shaders);
}

} // namespace lcf::vkc

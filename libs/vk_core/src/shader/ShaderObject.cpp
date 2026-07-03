#include "vk_core/shader/ShaderObject.h"
#include "vk_core/shader/entry.h"
#include "vk_core/manifest/DeviceExtensionManifest.h"
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


std::error_code ShaderObject::create(vk::Device device, const vk::ShaderCreateInfoEXT & info) noexcept
{
    try {
        auto result_value = device.createShaderEXTUnique(info);
        if (result_value.result != vk::Result::eSuccess) { return vk::make_error_code(result_value.result); }
        m_shader = std::move(result_value.value);
    } catch (const vk::SystemError & e) {
        return e.code();
    }
    m_stage = info.stage;
    return {};
}

} // namespace lcf::vkc

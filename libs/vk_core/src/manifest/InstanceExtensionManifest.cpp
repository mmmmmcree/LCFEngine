#include "vk_core/manifest/InstanceExtensionManifest.h"
#include <format>
#include <algorithm>
#include <ranges>

namespace stdr = std::ranges;

namespace lcf::vkc {

std::string InstanceExtensionManifest::getUnsupportedExtensionsMessage() const noexcept
{
    std::string message;
    auto supported_extension_props = vk::enumerateInstanceExtensionProperties();
    for (const auto & requried_ext_name : m_required_extensions) {
        auto found_it = stdr::find_if(supported_extension_props, [&requried_ext_name](const vk::ExtensionProperties &props) {
            return std::string(props.extensionName.data()) == requried_ext_name; });
        if (found_it != supported_extension_props.end()) { continue; }
        message += std::format("unsupported instance extension: {}\n", requried_ext_name);
    }
    return message;   
}   

} // namespace lcf::vkc


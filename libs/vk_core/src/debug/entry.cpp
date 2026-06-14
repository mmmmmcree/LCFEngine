#include "vk_core/debug/entry.h"
#include "vk_core/debug/debug_utils.h"
#include "vk_core/manifest/InstanceExtensionManifest.h"

namespace lcf::vkc::dbg {

void register_debug_utils(InstanceExtensionManifest &manifest) noexcept
{
    static constexpr std::array s_ext_names
    {
        vk::EXTDebugUtilsExtensionName,
    };
    manifest.addRequiredExtensions(s_ext_names);
    manifest.addExtensionEnableCallback(&enable_debug_utils);
}

} // namespace lcf::vkc::dbg
#include "vk_core/bootstrap/create_instance.h"
#include "vk_core/bootstrap/create_infos.h"
#include "vk_core/bootstrap/api_dispatch.h"
#include "vk_core/manifest/InstanceExtensionManifest.h"
#include "vk_core/error.h"

namespace stdr = std::ranges;
namespace stdv = std::views;

namespace {

using namespace lcf::vkc::bs;

vk::UniqueInstance create_instance_maythrow(const InstanceCreateInfo & create_info);

} // namespace

namespace lcf::vkc::bs {

std::expected<vk::UniqueInstance, std::error_code> create_instance(const InstanceCreateInfo &create_info) noexcept
{
    initialize_loader();
    vk::UniqueInstance instance {};
    try {
        instance = create_instance_maythrow(create_info);
    } catch (const vk::SystemError & e) {
        return std::unexpected(e.code());
    }
    if (not instance) { return std::unexpected(make_error_code(errc::no_suitable_instance)); }
    initialize_instance(instance.get());
    return instance;
}

} // namespace lcf::vkc::bs

namespace {

vk::UniqueInstance create_instance_maythrow(const InstanceCreateInfo &create_info)
{
    auto instance_layer_props = vk::enumerateInstanceLayerProperties() |
        stdv::filter([&](const vk::LayerProperties & layer) { return create_info.isLayerRequired(layer.layerName.data()); }) |
        stdr::to<std::vector>();
    if (instance_layer_props.size() != create_info.getRequiredInstanceLayerCount()) { return {}; }
    auto instance_extension_props = vk::enumerateInstanceExtensionProperties() |
        stdv::filter([&](const vk::ExtensionProperties & ext_props) { return create_info.isExtensionRequired(ext_props.extensionName.data()); }) |
        stdr::to<std::vector>();
    std::size_t ec = instance_extension_props.size(), ec_required = create_info.getRequiredInstanceExtensionCount();
    if (instance_extension_props.size() != create_info.getRequiredInstanceExtensionCount()) { return {}; }
    auto instance_layer_names_cstr = instance_layer_props |
        stdv::transform([](const vk::LayerProperties & layer) { return layer.layerName.data(); }) |
        stdr::to<std::vector>();
    auto instance_extension_names_cstr = instance_extension_props |
        stdv::transform([](const vk::ExtensionProperties & extension) { return extension.extensionName.data(); }) |
        stdr::to<std::vector>();

    vk::InstanceCreateInfo instance_create_info;
    instance_create_info.setPApplicationInfo(&create_info.getApplicationInfo())
        .setPEnabledLayerNames(instance_layer_names_cstr)
        .setPEnabledExtensionNames(instance_extension_names_cstr);
    return vk::createInstanceUnique(instance_create_info);
}

} // namespace
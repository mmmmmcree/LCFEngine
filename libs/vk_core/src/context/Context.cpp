#include "vk_core/context/Context.h"
#include "vk_core/bootstrap/api_dispatch.h"
#include <expected>

namespace stdr = std::ranges;
namespace stdv = std::views;

namespace {

using namespace lcf::vkc;

vk::UniqueInstance create_instance_maythrow(const ContextCreateInfo &create_info);

std::expected<vk::UniqueInstance, std::error_code> create_instance(const ContextCreateInfo &create_info) noexcept;

} // namespace

namespace lcf::vkc {

std::error_code Context::create(const ContextCreateInfo &create_info) noexcept
{
    bs::initialize_loader();
    auto expected_instance = create_instance(create_info);
    if (not expected_instance.has_value()) { return expected_instance.error(); }
    m_instance = std::move(expected_instance.value());
    bs::initialize_instance(this->getInstance());
    return {};
}

} // namespace lcf::vkc

namespace {

vk::UniqueInstance create_instance_maythrow(const ContextCreateInfo &create_info)
{
    auto instance_layer_properties = vk::enumerateInstanceLayerProperties() |
        stdv::filter([&](const vk::LayerProperties & layer) { return create_info.requiresLayer(layer.layerName.data()); }) |
        stdr::to<std::vector>();
    auto instance_extension_properties = vk::enumerateInstanceExtensionProperties() |
        stdv::filter([&](const vk::ExtensionProperties & extension) { return create_info.requiresExtension(extension.extensionName.data()); }) |
        stdr::to<std::vector>();
    auto instance_layer_names_cstr = instance_layer_properties |
        stdv::transform([](const vk::LayerProperties & layer) { return layer.layerName.data(); }) |
        stdr::to<std::vector>();
    auto instance_extension_names_cstr = instance_extension_properties |
        stdv::transform([](const vk::ExtensionProperties & extension) { return extension.extensionName.data(); }) |
        stdr::to<std::vector>();

    vk::InstanceCreateInfo instance_create_info;
    instance_create_info.setPApplicationInfo(&create_info.getApplicationInfo())
        .setPEnabledLayerNames(instance_layer_names_cstr)
        .setPEnabledExtensionNames(instance_extension_names_cstr);
    return vk::createInstanceUnique(instance_create_info);
}

std::expected<vk::UniqueInstance, std::error_code> create_instance(const ContextCreateInfo &create_info) noexcept
{
    try {
        return create_instance_maythrow(create_info);
    } catch (const vk::SystemError & e) {
        return std::unexpected(e.code());
    }
    return {};
}

} // namespace
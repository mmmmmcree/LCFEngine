#include "vk_core/context/Context.h"
#include "vk_core/details/instance_extensions/debug_utils.h"
#include "vk_core/details/api_dispatch.h"
#include <expected>

namespace stdr = std::ranges;
namespace stdv = std::views;

namespace {

using namespace lcf::vkc;

std::expected<vk::UniqueInstance, std::error_code> create_instance(const ContextCreateInfo &create_info) noexcept;

} // namespace

namespace lcf::vkc {

std::error_code Context::create(const ContextCreateInfo &create_info) noexcept
{
    details::initialize_loader();
    auto expected_instance = create_instance(create_info);
    if (not expected_instance.has_value()) { return expected_instance.error(); }
    m_instance = std::move(expected_instance.value());
    details::initialize_instance(this->getInstance());
    if (auto lease = details::enable_debug_utils(this->getInstance())) {
        m_extension_resource_leases.emplace_back(std::move(lease));
    }
    return {};
}

} // namespace lcf::vkc

namespace {

std::expected<vk::UniqueInstance, std::error_code> create_instance(const ContextCreateInfo &create_info) noexcept
{
    auto instance_layer_names = vk::enumerateInstanceLayerProperties() |
        stdv::transform([](const vk::LayerProperties & layer) { return layer.layerName.data(); })  |
        stdv::filter([&create_info](const char * layer_name) {
            return create_info.requiresLayer(layer_name);
        }) | stdr::to<std::vector>();
    auto instance_extension_names = vk::enumerateInstanceExtensionProperties() |
        stdv::transform([](const vk::ExtensionProperties & extension) { return extension.extensionName.data(); }) |
        stdv::filter([&create_info](const char * extension_name) {
            return create_info.requiresExtension(extension_name);
        }) | stdr::to<std::vector>();
    vk::InstanceCreateInfo instance_create_info;
    instance_create_info.setPApplicationInfo(&create_info.getApplicationInfo())
        .setPEnabledLayerNames(instance_layer_names)
        .setPEnabledExtensionNames(instance_extension_names);
    vk::UniqueInstance instance;
    try {
        instance = vk::createInstanceUnique(instance_create_info);
    } catch (const vk::SystemError & e) {
        return std::unexpected(e.code());
    }
    return instance;
}

} // namespace
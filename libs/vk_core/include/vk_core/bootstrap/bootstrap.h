#pragma once

#include "vk_core/config/FeatureDeclare.h"
#include "vk_core/context/Context.h"
#include <expected>
#include <functional>
#include <system_error>

namespace lcf::vkc::bs {

int prefer_most_queue_parallelism(vk::PhysicalDevice physical_device);

struct BootstrapConfig
{
    std::string_view app_name;
    uint32_t api_version = vk::ApiVersion13;
    std::span<const conf::FeatureDependency * const> feature_dependencies = {};
    std::function<int(vk::PhysicalDevice)> scorer = prefer_most_queue_parallelism;
};

std::expected<Context, std::error_code> bootstrap(const BootstrapConfig & config);

} // namespace lcf::vkc::bs

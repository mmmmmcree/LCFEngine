#include "vk_core/bootstrap/bootstrap.h"

#include <algorithm>
#include <ranges>

namespace {

using lcf::vkc::conf::FeatureDependency;
using lcf::vkc::conf::FeatureChain;

std::vector<const FeatureDependency *> flatten(std::span<const FeatureDependency * const> dependencies);
void log_feature_dependencies(std::span<const FeatureDependency * const> dependencies);
bool is_core(const FeatureDependency & dependency, uint32_t api_version) noexcept;
std::vector<const char *> instance_extension_union(
    std::span<const FeatureDependency * const> dependencies, uint32_t api_version);
std::vector<const char *> device_extension_union(
    std::span<const FeatureDependency * const> dependencies, uint32_t api_version);
FeatureChain requested_chain(std::span<const FeatureDependency * const> dependencies);
bool has_device_extensions(vk::PhysicalDevice physical_device, std::span<const char * const> names);
std::vector<const FeatureDependency *> supported_dependencies(
    vk::PhysicalDevice physical_device, std::span<const FeatureDependency * const> wished, uint32_t api_version);
std::vector<vk::DeviceQueueCreateInfo> all_queues_of_all_families(vk::PhysicalDevice physical_device);

} // namespace

namespace lcf::vkc::bs {

int prefer_most_queue_parallelism(vk::PhysicalDevice physical_device)
{
    // score by distinct family count; dedicated transfer/compute families weigh extra
    return 0;
}

std::expected<Context, std::error_code> bootstrap(const BootstrapConfig & config)
{
    auto wished = flatten(config.feature_dependencies);
    log_feature_dependencies(wished);

    // create vk::Instance with app_name/api_version + instance_extension_union(wished, ...),
    // wrap into Context (instance, debug messenger, instance dispatch)
    Context context;

    struct Candidate
    {
        vk::PhysicalDevice physical_device;
        std::vector<const FeatureDependency *> supported;
    };
    std::vector<Candidate> candidates;
    for (vk::PhysicalDevice physical_device : std::vector<vk::PhysicalDevice>{} /* context.physicalDevices() */) {
        candidates.push_back({physical_device, supported_dependencies(physical_device, wished, config.api_version)});
    }
    if (candidates.empty()) { return std::unexpected(std::make_error_code(std::errc::not_supported)); }

    auto & chosen = *std::ranges::max_element(candidates, {}, [&](const Candidate & candidate) {
        return std::pair(candidate.supported.size(), config.scorer(candidate.physical_device));
    });

    auto enabled = requested_chain(chosen.supported);
    // create vk::Device on chosen.physical_device:
    //   pNext      = &enabled.root()
    //   extensions = device_extension_union(chosen.supported, config.api_version)
    //   queues     = all_queues_of_all_families(chosen.physical_device)
    // freeze capability snapshot {api_version, enabled extensions/features} into the DeviceContext;
    // module create entry points (Swapchain::create, ...) check the snapshot and return an error
    // on absence — vkc provides capability, never guarantees device support; routing is upstairs.
    // populate the DeviceContext's QueueContexts — one (queue + timeline + submission counter)
    // per created queue — then build the QueueRole table via the collapse ladder:
    //   eCompute/eTransfer → dedicated family → spare graphics-family queue → eMainGraphics
    //   eSubGraphics       → second graphics-family queue → eMainGraphics
    // finally emplace the DeviceContext into context (sorted by score) and build the
    // DeviceRole table: eMain = strongest, eCompute = best remaining compute-capable
    // device, aliasing eMain on single-GPU machines; bootstrap creates one DeviceContext
    // for the chosen device, more can be added for multi-GPU

    return context;
}

} // namespace lcf::vkc::bs

namespace {

std::vector<const FeatureDependency *> flatten(std::span<const FeatureDependency * const> dependencies)
{
    std::vector<const FeatureDependency *> result;
    auto add = [&](this auto && self, const FeatureDependency * dependency_p) -> void {
        if (std::ranges::contains(result, dependency_p)) { return; }
        result.push_back(dependency_p);
        for (auto optional_p : dependency_p->optional) { self(optional_p); }
    };
    for (auto dependency_p : dependencies) { add(dependency_p); }
    return result;
}

void log_feature_dependencies(std::span<const FeatureDependency * const> dependencies)
{
    // engine logger: one line per dependency — extension names, core_since
}

bool is_core(const FeatureDependency & dependency, uint32_t api_version) noexcept
{
    return dependency.core_since != 0 and dependency.core_since <= api_version;
}

std::vector<const char *> instance_extension_union(
    std::span<const FeatureDependency * const> dependencies, uint32_t api_version)
{
    std::vector<const char *> result;
    for (auto dependency_p : dependencies) {
        if (is_core(*dependency_p, api_version)) { continue; }
        result.append_range(dependency_p->instance_extensions);
    }
    // dedup, then intersect with vk::enumerateInstanceExtensionProperties():
    // wish-list semantics — unavailable instance extensions are dropped, not fatal
    return result;
}

std::vector<const char *> device_extension_union(
    std::span<const FeatureDependency * const> dependencies, uint32_t api_version)
{
    std::vector<const char *> result;
    for (auto dependency_p : dependencies) {
        if (is_core(*dependency_p, api_version)) { continue; }
        result.append_range(dependency_p->device_extensions);
    }
    // dedup
    return result;
}

FeatureChain requested_chain(std::span<const FeatureDependency * const> dependencies)
{
    FeatureChain chain;
    for (auto dependency_p : dependencies) {
        for (const auto & feature : dependency_p->features) { feature.enable(chain); }
    }
    return chain;
}

bool has_device_extensions(vk::PhysicalDevice physical_device, std::span<const char * const> names)
{
    // names ⊆ physical_device.enumerateDeviceExtensionProperties()
    return true;
}

std::vector<const FeatureDependency *> supported_dependencies(
    vk::PhysicalDevice physical_device, std::span<const FeatureDependency * const> wished, uint32_t api_version)
{
    auto queried = requested_chain(wished);
    // physical_device.getFeatures2(&queried.root()) — the driver overwrites every
    // requested field with the supported value, so the same chain shape serves the query
    std::vector<const FeatureDependency *> supported;
    for (auto dependency_p : wished) {
        bool extensions_ok = is_core(*dependency_p, api_version)
            or has_device_extensions(physical_device, dependency_p->device_extensions);
        bool features_ok = std::ranges::all_of(dependency_p->features,
            [&](const auto & feature) { return feature.test(queried); });
        if (extensions_ok and features_ok) { supported.push_back(dependency_p); }
    }
    return supported;
}

std::vector<vk::DeviceQueueCreateInfo> all_queues_of_all_families(vk::PhysicalDevice physical_device)
{
    // one DeviceQueueCreateInfo per family, queueCount = family.queueCount —
    // full topology; role naming belongs to the render layer
    return {};
}

} // namespace

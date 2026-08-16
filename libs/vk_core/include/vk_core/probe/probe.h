#pragma once

#include "vk_core/bootstrap/info_structs.h"
#include "vk_core/error.h"
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <flat_set>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace lcf::vkc {

class DeviceExtensionManifest;
class InstanceExtensionManifest;

} // namespace lcf::vkc

namespace lcf::vkc::probe {

struct CapabilityOption
{
    using RegisterFunc = void (*)(InstanceExtensionManifest &, DeviceExtensionManifest &) noexcept;

    std::string_view type_header;
    RegisterFunc register_requirements = nullptr;
};

using CapabilityDescriptor = std::span<const CapabilityOption>;

class CapabilityRegistry
{
    using Self = CapabilityRegistry;
    using CapabilitySet = std::flat_set<const CapabilityDescriptor *>;
public:
    CapabilityRegistry(
        InstanceExtensionManifest & instance_manifest,
        DeviceExtensionManifest & device_manifest) noexcept :
        m_instance_manifest_p(&instance_manifest),
        m_device_manifest_p(&device_manifest) {}
public:
    template<typename Tag>
    requires requires(Tag tag) { { capability_descriptor(tag) } -> std::same_as<const CapabilityDescriptor &>; }
    Self & require()
    {
        const Tag tag {};
        const CapabilityDescriptor * descriptor_p = &capability_descriptor(tag);
        if (m_required_capabilities.emplace(descriptor_p).second) {
            register_probed(tag, *m_instance_manifest_p, *m_device_manifest_p);
        }
        return *this;
    }
    const CapabilitySet & getRequiredCapabilities() const noexcept { return m_required_capabilities; }
private:
    CapabilitySet m_required_capabilities;
    InstanceExtensionManifest * m_instance_manifest_p;
    DeviceExtensionManifest * m_device_manifest_p;
};

struct ProbeResult
{
    struct CapabilityResolution
    {
        const CapabilityDescriptor * capability = nullptr;
        std::size_t option_index = 0;
    };

    ProbeResult(
        const vk::PhysicalDeviceProperties & physical_device_properties,
        std::span<const CapabilityDescriptor * const> capability_descriptors,
        std::span<const std::size_t> option_selection);

    std::string name;
    uint32_t vendor_id = 0;
    uint32_t device_id = 0;
    uint32_t api_version = 0;
    std::vector<CapabilityResolution> capabilities;
};

std::expected<ProbeResult, Error> run(const bs::PhysicalDeviceSelectInfo & physical_device_select_info, const CapabilityRegistry & capabilities) noexcept;

} // namespace lcf::vkc::probe

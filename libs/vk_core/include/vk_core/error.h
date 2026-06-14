#pragma once

#include <system_error>

namespace lcf::vkc {

enum class errc
{
    no_error = 0,
    no_suitable_instance,
    no_suitable_physical_device,
    no_suitable_queue_family,
    preferred_device_type_anavailable,
    no_suitable_surface_format,
    no_suitable_present_mode,
    no_suitable_present_queue_family,
    main_device_role_not_configured,
};

std::error_code make_error_code(errc error) noexcept;

} // namespace lcf::vkc

template <>
struct std::is_error_code_enum<lcf::vkc::errc> : std::true_type {};

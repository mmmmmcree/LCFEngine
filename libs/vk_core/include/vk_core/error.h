#pragma once

#include <system_error>

namespace lcf::vkc {

enum class errc
{
    no_error = 0,
    no_suitable_instance,
    no_suitable_physical_device,
    no_suitable_queue_family,
    preferred_device_type_unavailable,
    no_suitable_surface_format,
    no_suitable_present_mode,
    no_suitable_present_queue_family,
    main_device_role_not_configured,
    surface_zero_size,
    missing_required_instance_layer,
    missing_required_instance_extension,
    missing_required_device_extension,
    missing_required_device_feature,
    present_skipped_for_resize,
    command_buffer_batch_exhausted,
    command_buffer_batch_queue_mismatch,
};

std::error_code make_error_code(errc error) noexcept;

} // namespace lcf::vkc

template <>
struct std::is_error_code_enum<lcf::vkc::errc> : std::true_type {};

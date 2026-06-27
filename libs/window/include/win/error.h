#pragma once

#include <system_error>

namespace lcf::win {

enum class errc
{
    no_error = 0,
    backend_init_failed,
    window_creation_failed,
    display_query_failed,
    native_handle_unavailable,
    metal_layer_creation_failed,
};

std::error_code make_error_code(errc error) noexcept;

} // namespace lcf::win

template <>
struct std::is_error_code_enum<lcf::win::errc> : std::true_type {};

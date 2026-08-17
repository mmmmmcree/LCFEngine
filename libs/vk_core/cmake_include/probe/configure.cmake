include_guard(GLOBAL)

function(vkc_configure_probe)
    set(_options)
    set(_one_value_args TARGET CONFIG_HEADER OUTPUT_HEADER)
    cmake_parse_arguments(PARSE_ARGV 0 VKC_PROBE
        "${_options}" "${_one_value_args}" "")

    if(VKC_PROBE_UNPARSED_ARGUMENTS)
        message(FATAL_ERROR
            "vkc_configure_probe received unknown arguments: ${VKC_PROBE_UNPARSED_ARGUMENTS}")
    endif()
    foreach(_required_arg IN ITEMS TARGET CONFIG_HEADER)
        if(NOT VKC_PROBE_${_required_arg})
            message(FATAL_ERROR "vkc_configure_probe requires ${_required_arg}")
        endif()
    endforeach()
    if(NOT TARGET ${VKC_PROBE_TARGET})
        message(FATAL_ERROR
            "vkc_configure_probe target does not exist: ${VKC_PROBE_TARGET}")
    endif()
    if(NOT TARGET vk_core)
        message(FATAL_ERROR "vkc_configure_probe requires the vk_core target")
    endif()

    get_filename_component(_config_header
        "${VKC_PROBE_CONFIG_HEADER}" ABSOLUTE
        BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
    if(NOT EXISTS "${_config_header}")
        message(FATAL_ERROR
            "Vulkan probe config header does not exist: ${_config_header}")
    endif()
    file(TO_CMAKE_PATH "${_config_header}" _config_header_for_cpp)

    if(VKC_PROBE_OUTPUT_HEADER)
        if(IS_ABSOLUTE "${VKC_PROBE_OUTPUT_HEADER}"
            OR VKC_PROBE_OUTPUT_HEADER MATCHES "(^|/)\\.\\.(/|$)")
            message(FATAL_ERROR
                "OUTPUT_HEADER must be a relative include path inside the generated directory")
        endif()
        set(_output_header "${VKC_PROBE_OUTPUT_HEADER}")
    else()
        get_filename_component(_output_header "${_config_header}" NAME)
    endif()

    get_property(_already_configured TARGET ${VKC_PROBE_TARGET}
        PROPERTY VKC_PROBE_CONFIGURED)
    if(_already_configured)
        message(FATAL_ERROR
            "vkc_configure_probe was already called for ${VKC_PROBE_TARGET}")
    endif()
    set_property(TARGET ${VKC_PROBE_TARGET} PROPERTY VKC_PROBE_CONFIGURED TRUE)

    set(_module_dir "${CMAKE_CURRENT_FUNCTION_LIST_DIR}")
    set(_generated_dir
        "${CMAKE_CURRENT_BINARY_DIR}/vkc_probe/${VKC_PROBE_TARGET}")
    set(_runner_source "${_generated_dir}/main.cpp")
    set(_profile "${_generated_dir}/profile.json")
    set(_generated_header "${_generated_dir}/${_output_header}")
    get_filename_component(_generated_header_dir "${_generated_header}" DIRECTORY)
    file(MAKE_DIRECTORY "${_generated_header_dir}")

    set(VKC_PROBE_CONFIG_HEADER "${_config_header_for_cpp}")
    configure_file(
        "${_module_dir}/main.cpp.in"
        "${_runner_source}"
        @ONLY)

    set(_probe_target "vkc_probe_${VKC_PROBE_TARGET}")
    add_executable(${_probe_target} EXCLUDE_FROM_ALL
        "${_runner_source}"
        "${_config_header}")
    target_link_libraries(${_probe_target} PRIVATE vk_core)

    add_custom_command(
        OUTPUT "${_generated_header}"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${_generated_header_dir}"
        COMMAND $<TARGET_FILE:${_probe_target}> "${_profile}"
        COMMAND ${CMAKE_COMMAND}
            "-DINPUT=${_profile}"
            "-DOUTPUT=${_generated_header}"
            "-DTEMPLATE=${_module_dir}/config.h.in"
            "-DSOURCE_CONFIG=${_config_header_for_cpp}"
            -P "${_module_dir}/generate_config.cmake"
        BYPRODUCTS
            "${_profile}"
        DEPENDS
            ${_probe_target}
            "${_config_header}"
            "${_module_dir}/config.h.in"
            "${_module_dir}/generate_config.cmake"
        COMMENT "Running Vulkan capability probe for ${VKC_PROBE_TARGET}"
        VERBATIM)

    target_sources(${VKC_PROBE_TARGET} PRIVATE
        "${_config_header}"
        "${_generated_header}")
    target_include_directories(${VKC_PROBE_TARGET} BEFORE PRIVATE
        "${_generated_dir}")
endfunction()

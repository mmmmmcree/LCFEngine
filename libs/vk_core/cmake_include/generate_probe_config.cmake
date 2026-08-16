foreach(_required_variable IN ITEMS INPUT OUTPUT TEMPLATE SOURCE_CONFIG)
    if(NOT DEFINED ${_required_variable})
        message(FATAL_ERROR
            "generate_probe_config.cmake requires ${_required_variable}")
    endif()
endforeach()

file(READ "${INPUT}" _profile_json)
string(JSON _schema_version GET "${_profile_json}" schema_version)
if(NOT _schema_version EQUAL 2)
    message(FATAL_ERROR
        "unsupported Vulkan probe schema version: ${_schema_version}")
endif()

set(CAPABILITY_INCLUDES "")
set(_selected_headers "")

string(JSON _capability_count LENGTH "${_profile_json}" capabilities)
if(_capability_count GREATER 0)
    math(EXPR _last_capability_index "${_capability_count} - 1")
    foreach(_index RANGE 0 ${_last_capability_index})
        string(JSON _type_header GET "${_profile_json}" capabilities ${_index} type_header)
        if(_type_header)
            string(APPEND CAPABILITY_INCLUDES
                "#include \"${_type_header}\"\n")
            string(APPEND _selected_headers " ${_type_header}")
        endif()
    endforeach()
endif()

get_filename_component(_output_directory "${OUTPUT}" DIRECTORY)
file(MAKE_DIRECTORY "${_output_directory}")
set(_temporary_output "${OUTPUT}.tmp")
configure_file("${TEMPLATE}" "${_temporary_output}" @ONLY)
execute_process(
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
        "${_temporary_output}" "${OUTPUT}"
    COMMAND_ERROR_IS_FATAL ANY)
file(REMOVE "${_temporary_output}")

string(JSON _device_name GET "${_profile_json}" physical_device name)
message(STATUS "Vulkan probe: ${_device_name};${_selected_headers}")

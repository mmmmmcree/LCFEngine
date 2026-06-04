# deps_conan.cmake — install dependencies via Conan 2.x.
#
# Generates a conanfile.txt with `[requires]` from each dep's "name/version",
# runs `conan install` with the CMakeDeps generator, and prepends the
# generated config dir to CMAKE_PREFIX_PATH so find_package(<name> CONFIG)
# resolves to Conan-installed packages.
#
# To avoid ABI mismatch between Conan-built libs and the consuming project,
# we synthesize a Conan profile that mirrors the active CMake compiler
# (instead of relying on the auto-detected default profile, which on MinGW
# boxes tends to pick gcc even when the active CMake preset is MSVC/clang-cl).
#
# We still ensure a `default` profile exists, because Conan 2.x looks one up
# as the build profile by default.
#
# The "features" field on dependencies.json entries is silently ignored
# (vcpkg-only concept; Conan options are per-recipe and not equivalent).

# --- Backend interface: detect ----------------------------------------------
#
# deps.cmake calls _conan_detect() during auto-detection. Conan has no
# pre-project setup — there's no toolchain to install — so no _conan_setup().

function(_conan_detect OUT_VAR)
    find_program(_c conan)
    if(NOT _c)
        set(${OUT_VAR} FALSE PARENT_SCOPE)
        return()
    endif()
    execute_process(
        COMMAND "${_c}" --version
        OUTPUT_VARIABLE _ver ERROR_QUIET
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    if(_ver MATCHES "Conan version 2")
        set(${OUT_VAR} TRUE PARENT_SCOPE)
    else()
        set(${OUT_VAR} FALSE PARENT_SCOPE)
    endif()
endfunction()

function(_conan_ensure_default_profile CONAN_EXE)
    execute_process(
        COMMAND ${CONAN_EXE} config home
        OUTPUT_VARIABLE _home
        OUTPUT_STRIP_TRAILING_WHITESPACE
        RESULT_VARIABLE _rc ERROR_QUIET
    )
    if(NOT _rc EQUAL 0)
        return()
    endif()
    if(EXISTS "${_home}/profiles/default")
        return()
    endif()
    message(STATUS "Conan: detecting default profile (first-time setup)")
    execute_process(COMMAND ${CONAN_EXE} profile detect RESULT_VARIABLE _rc)
    if(NOT _rc EQUAL 0)
        message(FATAL_ERROR "conan profile detect failed (rc=${_rc})")
    endif()
endfunction()

function(_conan_make_profile OUT_PATH)
    if(CMAKE_HOST_SYSTEM_NAME STREQUAL "Darwin")
        set(_os "Macos")
    else()
        set(_os "${CMAKE_HOST_SYSTEM_NAME}")
    endif()

    set(_lines "[settings]" "os=${_os}" "arch=x86_64")

    if(MSVC)
        math(EXPR _msvc_ver "${MSVC_TOOLSET_VERSION} + 50")
        if(_msvc_ver GREATER 194)
            set(_msvc_ver 194)
        endif()
        list(APPEND _lines
            "compiler=msvc"
            "compiler.version=${_msvc_ver}"
            "compiler.runtime=dynamic"
        )
    elseif(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        string(REGEX MATCH "^[0-9]+" _ver "${CMAKE_CXX_COMPILER_VERSION}")
        list(APPEND _lines
            "compiler=gcc"
            "compiler.version=${_ver}"
            "compiler.libcxx=libstdc++11"
        )
    elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        string(REGEX MATCH "^[0-9]+" _ver "${CMAKE_CXX_COMPILER_VERSION}")
        list(APPEND _lines
            "compiler=clang"
            "compiler.version=${_ver}"
            "compiler.libcxx=libstdc++11"
        )
    else()
        message(WARNING "Conan: unknown compiler ${CMAKE_CXX_COMPILER_ID}; "
                        "using auto-detected default profile (ABI mismatch risk)")
        set(${OUT_PATH} "" PARENT_SCOPE)
        return()
    endif()

    if(CMAKE_CXX_STANDARD)
        list(APPEND _lines "compiler.cppstd=${CMAKE_CXX_STANDARD}")
    endif()

    list(JOIN _lines "\n" _content)
    set(_path "${CMAKE_BINARY_DIR}/_conan_aggregate/profile")
    file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/_conan_aggregate")
    file(WRITE "${_path}" "${_content}\n")
    set(${OUT_PATH} "${_path}" PARENT_SCOPE)
endfunction()

function(deps_install_conan DEPS_JSON)
    find_program(CONAN_EXE conan REQUIRED)

    _conan_ensure_default_profile("${CONAN_EXE}")
    _conan_make_profile(_profile)

    string(JSON _len LENGTH "${DEPS_JSON}" dependencies)
    math(EXPR _last "${_len} - 1")

    set(_lines "[requires]")
    foreach(_i RANGE 0 ${_last})
        string(JSON _name GET "${DEPS_JSON}" dependencies ${_i} name)
        string(JSON _ver  ERROR_VARIABLE _e GET "${DEPS_JSON}" dependencies ${_i} version)
        if(_e OR NOT _ver)
            list(APPEND _lines "${_name}/[*]")
        else()
            list(APPEND _lines "${_name}/${_ver}")
        endif()
    endforeach()
    list(APPEND _lines "" "[generators]" "CMakeDeps")
    list(JOIN _lines "\n" _content)

    set(_dir         "${CMAKE_BINARY_DIR}/_conan_aggregate")
    set(_install_dir "${CMAKE_BINARY_DIR}/_conan_install")
    file(MAKE_DIRECTORY "${_dir}")
    file(WRITE "${_dir}/conanfile.txt" "${_content}\n")

    get_property(_is_multi GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG)
    if(_is_multi)
        set(_default_cfgs "Debug;Release")
    elseif(CMAKE_BUILD_TYPE)
        set(_default_cfgs "${CMAKE_BUILD_TYPE}")
    else()
        set(_default_cfgs "Release")
    endif()
    set(DEPS_CONAN_BUILD_TYPES "${_default_cfgs}" CACHE STRING
        "Build types to install via Conan (semicolon-separated)")

    set(_profile_args "")
    if(_profile)
        set(_profile_args -pr:a "${_profile}")
        message(STATUS "Conan: using profile matching CMake compiler -> ${_profile}")
    endif()

    foreach(_cfg IN LISTS DEPS_CONAN_BUILD_TYPES)
        message(STATUS "Conan install: -s build_type=${_cfg}")
        execute_process(
            COMMAND ${CONAN_EXE} install "${_dir}"
                    --output-folder=${_install_dir}
                    --build=missing
                    -s build_type=${_cfg}
                    ${_profile_args}
            RESULT_VARIABLE _rc
        )
        if(NOT _rc EQUAL 0)
            message(FATAL_ERROR "conan install failed for ${_cfg} (rc=${_rc})")
        endif()
    endforeach()

    list(APPEND CMAKE_PREFIX_PATH "${_install_dir}")
    set(CMAKE_PREFIX_PATH "${CMAKE_PREFIX_PATH}" PARENT_SCOPE)
endfunction()

list(APPEND _DEPS_BACKENDS "conan")
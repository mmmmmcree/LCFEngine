# deps.cmake — entry point for dependency management.
# Backend resolution (highest priority first):
#   1. -DDEPS_BACKEND=<name> on the command line
#   2. cacheVariables in the active CMake preset
#   3. unset / "auto" -> walk _DEPS_BACKENDS in include order, calling each
#      backend's _<name>_detect() until one returns TRUE.
#
# Each backend script under details/specific_dependencies_environment/ exposes:
#   list(APPEND _DEPS_BACKENDS "<name>")  — self-registration, top of file
#   _<name>_detect(OUT_VAR)               — required, sets OUT_VAR to TRUE/FALSE
#   _<name>_setup()                       — optional, pre-project side-effects
#   deps_install_<name>(DEPS_JSON)        — required, performs the install
#
# To change auto-detect priority, reorder the include() calls below. To add a
# new backend, drop a deps_<name>.cmake under details/... and include it here.
# deps.cmake itself contains no per-backend names.

include("${CMAKE_CURRENT_LIST_DIR}/pending_subdirs.cmake")

set(_DEPS_BACKENDS "")
include("${CMAKE_CURRENT_LIST_DIR}/details/specific_dependencies_environment/deps_vcpkg.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/details/specific_dependencies_environment/deps_conan.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/details/specific_dependencies_environment/deps_fetch.cmake")

# When ON, the vcpkg backend forces from-source rebuilds (no binary cache)
# and keeps extracted port sources under build/<preset>/vcpkg_installed/
# vcpkg/blds/. Lets you step into vcpkg-built libraries during debugging,
# at the cost of compile time on first configure. Conan/fetch ignore.
option(DEPS_KEEP_SOURCES
    "Force from-source rebuilds and keep extracted port sources under the build dir (vcpkg backend)"
    OFF)

# Resolve which backend to use exactly once, before project(). Auto-detection
# walks the include-order list each backend self-registered into.
function(_deps_resolve_backend)
    if(DEPS_BACKEND AND NOT DEPS_BACKEND STREQUAL "auto")
        message(STATUS "Dependency backend : ${DEPS_BACKEND} (user-specified)")
        return()
    endif()
    foreach(_b IN LISTS _DEPS_BACKENDS)
        cmake_language(CALL "_${_b}_detect" _ok)
        if(_ok)
            set(DEPS_BACKEND "${_b}" CACHE STRING
                "Dependency backend (one of: ${_DEPS_BACKENDS} | auto)" FORCE)
            message(STATUS "Dependency backend : ${_b} (auto-detected)")
            return()
        endif()
    endforeach()
    message(FATAL_ERROR
        "deps: no backend detected. Registered backends: ${_DEPS_BACKENDS}")
endfunction()

_deps_resolve_backend()

# Optional pre-project setup hook. Only vcpkg uses this today (to install its
# toolchain file before project() so per-target applocal-deploy works).
if(COMMAND "_${DEPS_BACKEND}_setup")
    cmake_language(CALL "_${DEPS_BACKEND}_setup")
endif()

# install_dependencies()
#   1. Walks each subdir registered via add_pending_subdir(). Before reading a
#      subdir's dependencies.json, include()s a sibling dependencies.cmake if
#      present — that file declares any cache options/variables the JSON's
#      "condition" expressions depend on (e.g. a backend selector). Cache
#      variables set there persist for the rest of configure.
#   2. Recursively finds every dependencies.json under it and merges their
#      "dependencies" arrays (dedup by name).
#   3. Each dependency may carry an optional "condition" — a CMake if()
#      expression string. Absent => always installed (backwards compatible).
#      Present and false => skipped. The key is stripped before merge so the
#      backends (vcpkg/conan/fetch) never see it.
#   4. Calls the active backend's deps_install_<b>(DEPS_JSON).
function(install_dependencies)
    get_pending_subdirs(_dirs)
    if(NOT _dirs)
        return()
    endif()

    # Pre-pass: let each registered subdir declare option/variable defaults its
    # dependencies.json conditions rely on, before any condition is evaluated.
    # Recursive to match the dependencies.json glob below — a lib may sit several
    # levels under a registered top-level dir (e.g. libs/window under libs).
    foreach(_d IN LISTS _dirs)
        file(GLOB_RECURSE _opt_files
             "${CMAKE_SOURCE_DIR}/${_d}/dependencies.cmake")
        foreach(_opt IN LISTS _opt_files)
            include("${_opt}")
        endforeach()
    endforeach()

    set(_seen_names "")
    set(_deps_inline "")
    foreach(_d IN LISTS _dirs)
        file(GLOB_RECURSE _files
             "${CMAKE_SOURCE_DIR}/${_d}/dependencies.json")
        foreach(_file IN LISTS _files)
            file(READ "${_file}" _content)
            string(JSON _len ERROR_VARIABLE _err
                   LENGTH "${_content}" dependencies)
            if(_err OR _len EQUAL 0)
                continue()   #- no "dependencies" array, or an empty one
            endif()
            math(EXPR _last "${_len} - 1")
            foreach(_i RANGE 0 ${_last})
                string(JSON _item GET "${_content}" dependencies ${_i})
                string(JSON _name GET "${_item}" name)
                if(_name IN_LIST _seen_names)
                    continue()
                endif()

                # Optional "condition": a CMake if() expression. A bare
                # if(${_cond}) does NOT work — expanding a variable into if()
                # doesn't re-tokenize operators like STREQUAL, so the string is
                # taken as a single value. cmake_language(EVAL) re-parses it as
                # fresh CMake code, which is the only correct evaluation here.
                # Conditions are repo-authored (trust level == CMakeLists.txt).
                string(JSON _cond ERROR_VARIABLE _e_cond
                       GET "${_item}" condition)
                if(NOT _e_cond AND NOT _cond STREQUAL "")
                    cmake_language(EVAL CODE
                        "if(${_cond})\n  set(_cond_keep TRUE)\nelse()\n  set(_cond_keep FALSE)\nendif()")
                    if(NOT _cond_keep)
                        continue()
                    endif()
                    # Strip the key so downstream backends never see it.
                    string(JSON _item REMOVE "${_item}" condition)
                endif()

                list(APPEND _seen_names "${_name}")
                if(_deps_inline)
                    string(APPEND _deps_inline ",${_item}")
                else()
                    set(_deps_inline "${_item}")
                endif()
            endforeach()
        endforeach()
    endforeach()

    set(_deps_json "{\"dependencies\":[${_deps_inline}]}")
    string(JSON _len ERROR_VARIABLE _err LENGTH "${_deps_json}" dependencies)
    if(_err OR _len EQUAL 0)
        return()
    endif()

    cmake_language(CALL "deps_install_${DEPS_BACKEND}" "${_deps_json}")

    # Propagate CMAKE_PREFIX_PATH / CMAKE_MODULE_PATH updates from the backend
    # through the function-scope chain to the caller.
    set(CMAKE_PREFIX_PATH "${CMAKE_PREFIX_PATH}" PARENT_SCOPE)
    set(CMAKE_MODULE_PATH "${CMAKE_MODULE_PATH}" PARENT_SCOPE)
endfunction()

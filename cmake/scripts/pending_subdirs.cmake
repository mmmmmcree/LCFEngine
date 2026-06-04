# pending_subdirs.cmake — two-phase subdirectory registration.
#
# Top-level CMakeLists registers module dirs with add_pending_subdir() before
# install_dependencies() runs. After dependencies are resolved (and any
# find_package-based config files are include()d), resolve_pending_subdirs()
# walks the registered list and calls CMake's built-in add_subdirectory() on
# each. The names are deliberately distinct from add_subdirectory() so call
# sites read unambiguously: registration vs. resolution.
#
# Other scripts can read the registered list via get_pending_subdirs() — for
# example, install_dependencies() iterates it to discover dependencies.json
# files.

function(add_pending_subdir PATH)
    set_property(GLOBAL APPEND PROPERTY _PROJECT_PENDING_SUBDIRS "${PATH}")
endfunction()

function(get_pending_subdirs OUT_VAR)
    get_property(_dirs GLOBAL PROPERTY _PROJECT_PENDING_SUBDIRS)
    set(${OUT_VAR} "${_dirs}" PARENT_SCOPE)
endfunction()

function(resolve_pending_subdirs)
    get_property(_dirs GLOBAL PROPERTY _PROJECT_PENDING_SUBDIRS)
    foreach(_d IN LISTS _dirs)
        add_subdirectory(${_d})
    endforeach()
endfunction()

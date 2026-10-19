# top_collect_deps: refresh deps.json from child manifests, preserving selected pins.
# top_list_dependency_versions: query recent tags without changing any manifest.
# top_sync_deps: write selected pins back to child manifests.
# top_update_deps_to_latest: collect, select by Git time, and synchronize all pins.
# Conflicts collected without a selection have selected_git_tag: null; sources
# record relative child paths and current_git_tag. Edit selected_git_tag before top_sync_deps.
# Registration only: configuring the project neither scans nor fetches dependencies.
foreach(deps_action IN ITEMS collect list sync latest)
    if(deps_action STREQUAL "collect")
        set(deps_target top_collect_deps)
        set(deps_comment "Collect child dependency manifests into deps.json")
    elseif(deps_action STREQUAL "list")
        set(deps_target top_list_dependency_versions)
        set(deps_comment "List recent versions of collected dependencies")
    elseif(deps_action STREQUAL "sync")
        set(deps_target top_sync_deps)
        set(deps_comment "Synchronize selected versions to child dependency manifests")
    else()
        set(deps_target top_update_deps_to_latest)
        set(deps_comment "Collect, select the newest tags, and synchronize dependency manifests")
    endif()
    add_custom_target(${deps_target}
        COMMAND "${CMAKE_COMMAND}" "-DROOT_DIR=${CMAKE_CURRENT_SOURCE_DIR}"
            "-DCACHE_DIR=${CMAKE_BINARY_DIR}/_deps/dependency-versions"
            "-DVERSION_SCRIPT=${CMAKE_CURRENT_SOURCE_DIR}/async/asio/cmake/dependency_versions.cmake"
            "-DACTION=${deps_action}" -P "${CMAKE_CURRENT_LIST_DIR}/sync_deps.cmake"
        COMMENT "${deps_comment}"
        USES_TERMINAL VERBATIM)
    set_target_properties(${deps_target} PROPERTIES FOLDER "top_dependencies")
endforeach()

add_test(NAME dependency_scripts
    COMMAND "${CMAKE_COMMAND}" "-DTEST_DIR=${CMAKE_BINARY_DIR}/dependency-script-tests"
        -P "${CMAKE_CURRENT_LIST_DIR}/tests/dependencies.cmake")

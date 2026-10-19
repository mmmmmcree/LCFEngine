include_guard(GLOBAL)
function(async_dependency_targets backend manifest)
    foreach(action list update)
        if(action STREQUAL "list")
            set(target ${backend}_list_dependency_versions)
            set(arguments)
        else()
            set(target ${backend}_update_deps_to_latest)
            set(arguments -DAPPLY=ON)
        endif()
        add_custom_target(${target}
            COMMAND "${CMAKE_COMMAND}" "-DDEPS_FILE=${manifest}"
                "-DCACHE_DIR=${CMAKE_BINARY_DIR}/_deps/dependency-versions" ${arguments}
                -P "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/dependency_versions.cmake"
            USES_TERMINAL VERBATIM)
        set_target_properties(${target} PROPERTIES FOLDER "async/${backend}/dependencies")
    endforeach()
endfunction()

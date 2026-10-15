set(async_deps_file "${CMAKE_CURRENT_LIST_DIR}/../deps.json")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${async_deps_file}")
file(READ "${async_deps_file}" async_deps_json)
string(JSON async_dependency_count LENGTH "${async_deps_json}" dependencies)
if(async_dependency_count EQUAL 0)
    return()
endif()

include(ExternalProject)
math(EXPR async_last_dependency "${async_dependency_count} - 1")
foreach(async_index RANGE 0 ${async_last_dependency})
    string(JSON async_dependency GET "${async_deps_json}" dependencies ${async_index})
    string(JSON async_name GET "${async_dependency}" name)
    string(JSON async_repository GET "${async_dependency}" git)
    string(JSON async_revision GET "${async_dependency}" git_tag)
    string(JSON async_target ERROR_VARIABLE async_target_error GET "${async_dependency}" target)
    if(async_target_error)
        set(async_target "${async_name}::${async_name}")
    endif()
    if(TARGET "${async_target}")
        continue()
    endif()

    string(TOUPPER "${async_name}" async_upper_name)
    set(async_override "FETCHCONTENT_SOURCE_DIR_${async_upper_name}")
    set(${async_override} "" CACHE PATH "Use an existing ${async_name} source tree")
    if(${async_override})
        get_filename_component(async_source_dir "${${async_override}}" ABSOLUTE BASE_DIR "${CMAKE_SOURCE_DIR}")
        if(NOT IS_DIRECTORY "${async_source_dir}")
            message(FATAL_ERROR "${async_override} must point to an existing source directory: ${async_source_dir}")
        endif()
        add_custom_target(async_${async_name}_download)
    else()
        set(async_source_dir "${CMAKE_BINARY_DIR}/_deps/async-${async_name}/source")
        ExternalProject_Add(async_${async_name}_download
            PREFIX "${CMAKE_BINARY_DIR}/_deps/async-${async_name}"
            SOURCE_DIR "${async_source_dir}"
            GIT_REPOSITORY "${async_repository}"
            GIT_TAG "${async_revision}"
            CONFIGURE_COMMAND ""
            BUILD_COMMAND ""
            INSTALL_COMMAND ""
            EXCLUDE_FROM_ALL TRUE
        )
    endif()
    set_target_properties(async_${async_name}_download PROPERTIES FOLDER "async/dependencies")
    set(${async_name}_SOURCE_DIR "${async_source_dir}")
endforeach()

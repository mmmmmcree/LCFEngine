project(window)

# LCF_WINDOW_BACKEND is already in the cache (declared in dependencies.cmake,
# auto-included by the deps system before install_dependencies()). Default SDL3.

file(GLOB_RECURSE HEADERS "include/*.h")

add_library(${PROJECT_NAME} SHARED
    ${HEADERS}
    src/error.cpp
)

if(WIN32)
    set_target_properties(${PROJECT_NAME} PROPERTIES
        WINDOWS_EXPORT_ALL_SYMBOLS ON
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/$<CONFIG>
        ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib/$<CONFIG>
    )
endif()

target_include_directories(${PROJECT_NAME}
    PUBLIC
        $<INSTALL_INTERFACE:include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(${PROJECT_NAME}
    PUBLIC
        utilities_core
)

# Compile-time single backend: only the selected backend's directory is added,
# so only its .cpp compiles and only its dependency links. The other backend
# directory is never touched.
string(TOLOWER "${LCF_WINDOW_BACKEND}" _backend_dir)
add_subdirectory(src/${_backend_dir})

message(STATUS "window backend     : ${LCF_WINDOW_BACKEND}")

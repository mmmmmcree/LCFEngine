project(hello_compat_swapchain)

file(GLOB_RECURSE SOURCES "src/*.cpp")

include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}/include
)
add_executable(${PROJECT_NAME} ${SOURCES})
find_package(glfw3 CONFIG REQUIRED)
target_link_libraries(${PROJECT_NAME}
    PRIVATE
        vk_core
        utilities_log
        glfw
)

set_target_properties(${PROJECT_NAME} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

# Expose the platform-native window handle accessors in glfw3native.h. These
# must be defined before that header is parsed; injecting them here guarantees
# it without relying on include order in the source. On Windows, NOMINMAX /
# WIN32_LEAN_AND_MEAN tame the <windows.h> that glfw3native.h pulls in.
if(WIN32)
    target_compile_definitions(${PROJECT_NAME} PRIVATE
        NOMINMAX
        WIN32_LEAN_AND_MEAN
        GLFW_EXPOSE_NATIVE_WIN32)
elseif(APPLE)
    target_compile_definitions(${PROJECT_NAME} PRIVATE
        GLFW_EXPOSE_NATIVE_COCOA)
else()
    target_compile_definitions(${PROJECT_NAME} PRIVATE
        GLFW_EXPOSE_NATIVE_X11
        GLFW_EXPOSE_NATIVE_WAYLAND)
endif()

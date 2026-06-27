find_package(glfw3 CONFIG REQUIRED)

file(GLOB_RECURSE SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")
target_sources(window PRIVATE ${SOURCES})

target_compile_definitions(window PRIVATE USE_GLFW=1)
target_link_libraries(window PRIVATE glfw)

# Expose the platform-native window handle accessors in glfw3native.h. These
# must be defined before that header is parsed; injecting them here guarantees
# it without relying on include order in the source. On Windows, NOMINMAX /
# WIN32_LEAN_AND_MEAN tame the <windows.h> that glfw3native.h pulls in.
if(WIN32)
    target_compile_definitions(window PRIVATE
        NOMINMAX
        WIN32_LEAN_AND_MEAN
        GLFW_EXPOSE_NATIVE_WIN32)
elseif(APPLE)
    target_compile_definitions(window PRIVATE
        GLFW_EXPOSE_NATIVE_COCOA)
else()
    target_compile_definitions(window PRIVATE
        GLFW_EXPOSE_NATIVE_X11
        GLFW_EXPOSE_NATIVE_WAYLAND)
endif()

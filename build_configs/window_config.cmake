set(config_target "window_config")

set(WINDOW_BACKEND_OPTIONS
    "SDL3"
    "SDL2"
    "GLFW"
)
set(WINDOW_BACKEND_DEFAULT "SDL3")

set(WINDOW_BACKEND "${WINDOW_BACKEND_DEFAULT}" CACHE STRING "Select window backend")
set_property(CACHE WINDOW_BACKEND PROPERTY STRINGS ${WINDOW_BACKEND_OPTIONS})
# 创建接口库
add_library(${config_target} INTERFACE)

# 根据选择的后端进行配置
if(WINDOW_BACKEND STREQUAL "SDL3")
    target_compile_definitions(${config_target} INTERFACE USE_SDL3=1)
    find_package(SDL3 REQUIRED)
    target_link_libraries(${config_target} INTERFACE SDL3::SDL3)
elseif(WINDOW_BACKEND STREQUAL "SDL2")
    target_compile_definitions(${config_target} INTERFACE USE_SDL2=1)
    find_package(SDL2 REQUIRED)
    target_link_libraries(${config_target} INTERFACE SDL2::SDL2)
elseif(WINDOW_BACKEND STREQUAL "GLFW")
    target_compile_definitions(${config_target} INTERFACE USE_GLFW=1)
    find_package(glfw3 REQUIRED)
    target_link_libraries(${config_target} INTERFACE glfw)
endif()
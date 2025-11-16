option(USE_VULKAN "Enable Vulkan rendering support" ON)
option(USE_OPENGL "Enable OpenGL rendering support" OFF)

set(config_target "graphic_api_config")

add_library(${config_target} INTERFACE)

set(DEFINITION_LIST "")

if(USE_VULKAN)
    list(APPEND DEFINITION_LIST "USE_VULKAN=1")
endif()

if(USE_OPENGL)
    list(APPEND DEFINITION_LIST "USE_OPENGL=1")
endif()

if(DEFINITION_LIST)
    target_compile_definitions(${config_target} INTERFACE ${DEFINITION_LIST})
endif()

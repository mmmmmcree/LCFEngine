cmake_minimum_required(VERSION 3.16) 
project(LCFEngine LANGUAGES CXX)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/build_configs")
set(CONFIG_MODULES
    global_config
    graphic_api_config
    window_config
)
foreach(module ${CONFIG_MODULES})
    include(${module})
endforeach()

add_subdirectory(input)
add_subdirectory(GUI) 
add_subdirectory(math)  
add_subdirectory(utilities)
add_subdirectory(RenderEngine)
add_subdirectory(examples)  
add_subdirectory(core)  
add_subdirectory(Modules) 
add_subdirectory(Systems)    
  
    
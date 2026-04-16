project(RenderEngine)

file(GLOB_RECURSE HEADERS "include/*.h")
file(GLOB_RECURSE SOURCES "src/*.cpp") 

add_library(${PROJECT_NAME} STATIC
    ${HEADERS}
    ${SOURCES} 
)
     
find_package(Boost CONFIG REQUIRED COMPONENTS container functional align)
find_package(assimp CONFIG REQUIRED)
     
include_directories( 
  ${CMAKE_CURRENT_SOURCE_DIR}/include 
)     

target_include_directories(${PROJECT_NAME}
    PUBLIC 
        $<INSTALL_INTERFACE:include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    PRIVATE 
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)          

target_link_libraries(${PROJECT_NAME}
    PUBLIC   
        graphic_api_config
        containers
        math
        utilities 
        image
        GUI 
        core 
        Boost::container Boost::functional Boost::align
        shader_core
        render_assets
        Systems
    PRIVATE
)  

if(USE_VULKAN)
    find_package(Vulkan REQUIRED)
    find_package(VulkanMemoryAllocator CONFIG REQUIRED)
    if(DYNAMIC_LOAD_GRAPHICS_API)
        target_compile_definitions(${PROJECT_NAME} PUBLIC
            VULKAN_HPP_DISPATCH_LOADER_DYNAMIC=1
            VK_NO_PROTOTYPES
        )
        target_compile_definitions(${PROJECT_NAME} PRIVATE
            VMA_STATIC_VULKAN_FUNCTIONS=0
            VMA_DYNAMIC_VULKAN_FUNCTIONS=1
        )
        target_link_libraries(${PROJECT_NAME} PRIVATE Vulkan::Headers)
    else()
        target_link_libraries(${PROJECT_NAME} PRIVATE Vulkan::Vulkan)
    endif()
    target_link_libraries(${PROJECT_NAME} PRIVATE GPUOpen::VulkanMemoryAllocator)
endif()
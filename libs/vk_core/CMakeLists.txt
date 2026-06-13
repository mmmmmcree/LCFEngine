project(vk_core)

file(GLOB_RECURSE HEADERS "include/*.h")
file(GLOB_RECURSE SOURCES "src/*.cpp") 

add_library(${PROJECT_NAME} SHARED
    ${HEADERS}
    ${SOURCES} 
)

if(WIN32)
    set_target_properties(${PROJECT_NAME} PROPERTIES
        WINDOWS_EXPORT_ALL_SYMBOLS ON
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/$<CONFIG>
        ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib/$<CONFIG>
    )    
endif()

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
        utilities
    PRIVATE
)

find_package(Vulkan REQUIRED)
find_package(VulkanMemoryAllocator CONFIG REQUIRED)
if(WIN32)
    target_compile_definitions(${PROJECT_NAME} PRIVATE VK_USE_PLATFORM_WIN32_KHR NOMINMAX)
elseif(APPLE)
    target_compile_definitions(${PROJECT_NAME} PRIVATE VK_USE_PLATFORM_METAL_EXT)
elseif(UNIX)
    target_compile_definitions(${PROJECT_NAME} PRIVATE VK_USE_PLATFORM_XCB_KHR VK_USE_PLATFORM_WAYLAND_KHR)
endif()
target_compile_definitions(${PROJECT_NAME} PUBLIC
    VULKAN_HPP_DISPATCH_LOADER_DYNAMIC=1
    VK_NO_PROTOTYPES
)
option(LCF_VKC_USE_DEVICE_DISPATCH "vk_core: driver-direct device-level dispatch (may blind some profilers)" OFF)
if(LCF_VKC_USE_DEVICE_DISPATCH)
    target_compile_definitions(${PROJECT_NAME} PRIVATE LCF_VKC_USE_DEVICE_DISPATCH)
endif()
target_compile_definitions(${PROJECT_NAME} PRIVATE
    VMA_STATIC_VULKAN_FUNCTIONS=0
    VMA_DYNAMIC_VULKAN_FUNCTIONS=1
)
target_link_libraries(${PROJECT_NAME} PUBLIC Vulkan::Headers)
target_link_libraries(${PROJECT_NAME} PUBLIC GPUOpen::VulkanMemoryAllocator)
#todo link libraries in private once the encaptulation is done
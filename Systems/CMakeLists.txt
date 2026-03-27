project(Systems)

file(GLOB_RECURSE HEADERS "include/*.h")
file(GLOB_RECURSE SOURCES "src/*.cpp")
  
add_library(${PROJECT_NAME} STATIC
    ${HEADERS}
    ${SOURCES}
)
   
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
link_directories(${Vulkan_LIBRARY})
target_link_libraries(${PROJECT_NAME}
    PUBLIC  
        math
        utilities 
        core
    PRIVATE
)
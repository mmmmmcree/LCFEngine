project(ecs_transform_test)

file(GLOB_RECURSE SOURCES "src/*.cpp")

find_package(Tracy CONFIG REQUIRED)

include_directories(  
  ${CMAKE_CURRENT_SOURCE_DIR}/include 
)
add_executable(${PROJECT_NAME} ${SOURCES})

target_link_libraries(${PROJECT_NAME}
    PRIVATE 
        core
        utilities
        math
        profiling
)

set_target_properties(${PROJECT_NAME} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)
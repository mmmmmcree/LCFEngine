project(coroutines_test)

file(GLOB_RECURSE SOURCES "src/*.cpp")

find_package(Boost REQUIRED COMPONENTS beast)
find_package(OpenSSL REQUIRED)

include_directories(  
  ${CMAKE_CURRENT_SOURCE_DIR}/include 
)
add_executable(${PROJECT_NAME} ${SOURCES})

target_link_libraries(${PROJECT_NAME}
    PRIVATE 
        core
        utilities
        profiling
        Boost::beast
        OpenSSL::SSL OpenSSL::Crypto
)

set_target_properties(${PROJECT_NAME} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)
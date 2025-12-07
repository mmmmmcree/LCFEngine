project(coroutines_test)
add_compile_options("/bigobj")

file(GLOB_RECURSE SOURCES "src/*.cpp")

find_package(Boost REQUIRED COMPONENTS beast url)
find_package(OpenSSL REQUIRED)
find_package(CLI11 CONFIG REQUIRED)

include_directories(  
  ${CMAKE_CURRENT_SOURCE_DIR}/include 
)
add_executable(${PROJECT_NAME} ${SOURCES})

target_link_libraries(${PROJECT_NAME}
    PRIVATE 
        core
        utilities
        input
        profiling
        Boost::beast
        OpenSSL::SSL OpenSSL::Crypto
        CLI11::CLI11
        Boost::url
)

set_target_properties(${PROJECT_NAME} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)
cmake_minimum_required(VERSION 3.28)

project(LCFCppLibraries LANGUAGES CXX)

set_property(GLOBAL PROPERTY USE_FOLDERS ON)
if(NOT CMAKE_SOURCE_DIR STREQUAL CMAKE_BINARY_DIR AND NOT EXISTS "${CMAKE_BINARY_DIR}/.gitignore")
    file(WRITE "${CMAKE_BINARY_DIR}/.gitignore" "*\n")
endif()

enable_testing()

include(cmake/dependencies.cmake)

option(LCF_LIBS_ENABLE_ASYNC "Enable the async library" ON)
option(LCF_LIBS_BUILD_EXAMPLES "Build collection examples" ON)
if(LCF_LIBS_ENABLE_ASYNC)
    add_subdirectory(async)
endif()
if(LCF_LIBS_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

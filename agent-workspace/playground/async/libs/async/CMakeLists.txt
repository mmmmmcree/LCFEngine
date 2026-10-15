cmake_minimum_required(VERSION 3.28)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    project(LCFAsync LANGUAGES CXX)
    enable_testing()
endif()

add_subdirectory(asio)

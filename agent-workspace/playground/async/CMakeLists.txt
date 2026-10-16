cmake_minimum_required(VERSION 3.28)

project(AsyncPlayground LANGUAGES CXX)

enable_testing()

include(cmake/dependencies.cmake)

add_subdirectory(libs/async)
add_subdirectory(examples)

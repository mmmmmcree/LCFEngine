cmake_minimum_required(VERSION 3.16) 
project(LCFEngine LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(BUILD_SHARED_LIBS ON)  
add_compile_options("$<$<C_COMPILER_ID:MSVC>:/utf-8>")
add_compile_options("$<$<CXX_COMPILER_ID:MSVC>:/utf-8>")

add_subdirectory(math)  
add_subdirectory(utilities)
add_subdirectory(RenderEngine)
add_subdirectory(WindowSystem)
add_subdirectory(examples)  
add_subdirectory(core)  
add_subdirectory(Modules) 
  
  
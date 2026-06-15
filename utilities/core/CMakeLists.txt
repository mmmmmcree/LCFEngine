project(utilities_core)

file(GLOB_RECURSE HEADERS "include/*.h")

add_library(${PROJECT_NAME} INTERFACE)
target_sources(${PROJECT_NAME} INTERFACE ${HEADERS})

find_path(HALF_INCLUDE_DIRS "half.hpp")

target_include_directories(${PROJECT_NAME}
  INTERFACE
      $<INSTALL_INTERFACE:include>
      $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
      ${HALF_INCLUDE_DIRS}
)

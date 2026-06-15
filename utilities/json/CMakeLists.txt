project(utilities_json)

file(GLOB_RECURSE HEADERS "include/*.h")

add_library(${PROJECT_NAME} INTERFACE)
target_sources(${PROJECT_NAME} INTERFACE ${HEADERS})

find_package(nlohmann_json CONFIG REQUIRED)

target_include_directories(${PROJECT_NAME}
  INTERFACE
      $<INSTALL_INTERFACE:include>
      $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)

target_link_libraries(${PROJECT_NAME}
  INTERFACE
    utilities_core
    nlohmann_json::nlohmann_json
)

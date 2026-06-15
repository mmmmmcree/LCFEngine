project(utilities)

add_subdirectory(core)
add_subdirectory(enums)
add_subdirectory(log)
add_subdirectory(json)

add_library(${PROJECT_NAME} INTERFACE)
target_link_libraries(${PROJECT_NAME}
  INTERFACE
    utilities_core
    utilities_enums
    utilities_log
    utilities_json
)

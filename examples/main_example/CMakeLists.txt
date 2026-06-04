project(main_example)

file(GLOB_RECURSE SOURCES "src/*.cpp")

include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}/include
)
add_executable(${PROJECT_NAME} ${SOURCES})

target_link_libraries(main_example
    PRIVATE
        RenderEngine
        camera_controllers
        Systems
        profiling
)

set_target_properties(main_example PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)


if(LCF_DEPLOY)
    target_compile_definitions(${PROJECT_NAME} PRIVATE
        LCF_EXAMPLES_ASSETS_DIR="./assets"
    )
    file(GLOB_RECURSE MAIN_EXAMPLE_ASSETS CONFIGURE_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/../assets/*"
    )
    set(MAIN_EXAMPLE_ASSET_DIR "${CMAKE_BINARY_DIR}/bin/$<CONFIG>/assets")
    add_custom_target(main_example_assets
        COMMAND ${CMAKE_COMMAND} -E make_directory "${MAIN_EXAMPLE_ASSET_DIR}"
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_CURRENT_SOURCE_DIR}/../assets"
            "${MAIN_EXAMPLE_ASSET_DIR}"
        DEPENDS ${MAIN_EXAMPLE_ASSETS}
        COMMENT "Copying assets for main_example (deploy mode)"
        VERBATIM
    )
    add_dependencies(main_example main_example_assets)
else()
    target_compile_definitions(${PROJECT_NAME} PRIVATE
        LCF_EXAMPLES_ASSETS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/../assets"
    )
endif()

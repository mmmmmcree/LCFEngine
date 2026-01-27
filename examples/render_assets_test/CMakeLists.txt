project(render_assets_test)

file(GLOB_RECURSE SOURCES "src/*.cpp")

include_directories(  
  ${CMAKE_CURRENT_SOURCE_DIR}/include 
)
add_executable(${PROJECT_NAME} ${SOURCES})


target_link_libraries(${PROJECT_NAME}
    PRIVATE 
        core
        utilities
        camera_controllers
        render_assets
)

add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E echo "Target file directory: $<TARGET_FILE_DIR:${PROJECT_NAME}>"
    COMMAND ${CMAKE_COMMAND} -E echo "Source assets directory: ${CMAKE_CURRENT_SOURCE_DIR}/../assets"
    COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_FILE_DIR:${PROJECT_NAME}>/assets
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_CURRENT_SOURCE_DIR}/../assets
        $<TARGET_FILE_DIR:${PROJECT_NAME}>/assets
    COMMENT "Copying assets to $<TARGET_FILE_DIR:${PROJECT_NAME}>/assets"
    VERBATIM
) 


set_target_properties(${PROJECT_NAME} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)
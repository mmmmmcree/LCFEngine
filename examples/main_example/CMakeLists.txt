project(main_example)

add_executable(main_example main.cpp)

target_link_libraries(main_example
    PRIVATE
        RenderEngine 
        Modules
        Systems
)

add_custom_command(TARGET main_example POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E echo "Target file directory: $<TARGET_FILE_DIR:main_example>"
    COMMAND ${CMAKE_COMMAND} -E echo "Source assets directory: ${CMAKE_CURRENT_SOURCE_DIR}/../assets"
    COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_FILE_DIR:main_example>/assets
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_CURRENT_SOURCE_DIR}/../assets
        $<TARGET_FILE_DIR:main_example>/assets
    COMMENT "Copying assets to $<TARGET_FILE_DIR:main_example>/assets"
    VERBATIM
) 

# if(MSVC)
#     target_compile_options(${PROJECT_NAME} PRIVATE
#         /permissive-
#         /Zc:__cplusplus
#         /Wall
#         /Zc:forScope
#         /Zc:inline
#         /Zc:strictStrings
#         /Zc:throwingNew
#         /Zc:referenceBinding
#         /Zc:noexceptTypes
#         /Zc:ternary
#         /analyze
#     )
# endif() 

set_target_properties(main_example PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)
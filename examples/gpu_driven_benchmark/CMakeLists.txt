project(gpu_driven_benchmark)

file(GLOB_RECURSE SOURCES "src/*.cpp")

include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}/include
)

add_executable(${PROJECT_NAME} ${SOURCES})

target_link_libraries(${PROJECT_NAME}
    PRIVATE
        RenderEngine
        camera_controllers
        Systems
        profiling
)

# Reuse examples/assets like main_example does.
file(GLOB_RECURSE GPU_DRIVEN_BENCHMARK_ASSETS CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/../assets/*"
)

set(GPU_DRIVEN_BENCHMARK_ASSET_DIR "${CMAKE_BINARY_DIR}/bin/$<CONFIG>/assets")

add_custom_target(${PROJECT_NAME}_assets
    COMMAND ${CMAKE_COMMAND} -E make_directory "${GPU_DRIVEN_BENCHMARK_ASSET_DIR}"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/../assets"
        "${GPU_DRIVEN_BENCHMARK_ASSET_DIR}"
    DEPENDS ${GPU_DRIVEN_BENCHMARK_ASSETS}
    COMMENT "Copying examples/assets for gpu_driven_benchmark"
    VERBATIM
)

# Also copy benchmark-private shaders (when present) into <bin>/<Config>/assets/shaders
# so that the same path prefix `assets/shaders/...` works at runtime.
file(GLOB_RECURSE GPU_DRIVEN_BENCHMARK_PRIVATE_SHADERS CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/shaders/*"
)
if(GPU_DRIVEN_BENCHMARK_PRIVATE_SHADERS)
    add_custom_target(${PROJECT_NAME}_private_shaders
        COMMAND ${CMAKE_COMMAND} -E make_directory "${GPU_DRIVEN_BENCHMARK_ASSET_DIR}/shaders"
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_CURRENT_SOURCE_DIR}/shaders"
            "${GPU_DRIVEN_BENCHMARK_ASSET_DIR}/shaders"
        DEPENDS ${GPU_DRIVEN_BENCHMARK_PRIVATE_SHADERS}
        COMMENT "Copying gpu_driven_benchmark private shaders"
        VERBATIM
    )
    add_dependencies(${PROJECT_NAME} ${PROJECT_NAME}_private_shaders)
endif()

add_dependencies(${PROJECT_NAME} ${PROJECT_NAME}_assets)

set_target_properties(${PROJECT_NAME} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

# ----------------------------------------------------------------------------
# Tracy 是 SHARED dll，profiling target 又是 INTERFACE 库，CMake 的 vcpkg
# applocal 自动 dll-deploy 在这种 INTERFACE 透传链路下不一定触发，导致 exe
# 同目录缺少 TracyClient.dll → STATUS_DLL_NOT_FOUND (0xC0000135)。
# 显式拷一份过去（如果存在）。
# ----------------------------------------------------------------------------
if(WIN32 AND TARGET Tracy::TracyClient)
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "$<TARGET_FILE:Tracy::TracyClient>"
            "$<TARGET_FILE_DIR:${PROJECT_NAME}>"
        COMMENT "Copying TracyClient.dll next to ${PROJECT_NAME}.exe"
        VERBATIM
    )
endif()

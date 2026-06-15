project(examples)

add_subdirectory(main_example)
add_subdirectory(ecs_transform_test)
# coroutines_test excluded: uses Boost.Beast (hard-depends on boost::asio),
# which cannot coexist with the standalone Asio the engine migrated to.
# add_subdirectory(coroutines_test)
add_subdirectory(render_assets_test)
add_subdirectory(resource_system_test)
add_subdirectory(gpu_driven_benchmark)
add_subdirectory(shader_core_test)
add_subdirectory(vkc_test)

find_package(SDL3 REQUIRED)

file(GLOB_RECURSE SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")
target_sources(window PRIVATE ${SOURCES})

target_compile_definitions(window PRIVATE USE_SDL3=1)
target_link_libraries(window PRIVATE SDL3::SDL3)

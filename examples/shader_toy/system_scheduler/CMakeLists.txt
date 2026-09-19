add_library(shader_toy_system_scheduler SHARED
    src/SystemScheduler.cpp
)

file(GLOB_RECURSE HEADERS CONFIGURE_DEPENDS "include/*.h")
target_sources(shader_toy_system_scheduler PRIVATE ${HEADERS})

target_include_directories(shader_toy_system_scheduler
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

if(WIN32)
    set_target_properties(shader_toy_system_scheduler PROPERTIES
        WINDOWS_EXPORT_ALL_SYMBOLS ON
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/$<CONFIG>
        ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib/$<CONFIG>
    )
endif()

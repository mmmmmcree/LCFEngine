add_library(shader_toy_task_system SHARED
    src/TaskSystem.cpp
)

find_package(asio CONFIG REQUIRED)

file(GLOB_RECURSE HEADERS CONFIGURE_DEPENDS "include/*.h")
target_sources(shader_toy_task_system PRIVATE ${HEADERS})

target_include_directories(shader_toy_task_system
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(shader_toy_task_system
    PUBLIC
        shader_toy_event
        asio::asio
)

if(WIN32)
    target_link_libraries(shader_toy_task_system
        PUBLIC
            ws2_32
            mswsock
    )
    set_target_properties(shader_toy_task_system PROPERTIES
        WINDOWS_EXPORT_ALL_SYMBOLS ON
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/$<CONFIG>
        ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib/$<CONFIG>
    )
endif()

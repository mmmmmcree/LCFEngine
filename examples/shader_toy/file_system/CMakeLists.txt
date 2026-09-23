add_library(shader_toy_file_system SHARED
    src/FileSystem.cpp
    src/FileWatcher.cpp
)

file(GLOB_RECURSE HEADERS CONFIGURE_DEPENDS
    "include/*.h"
    "src/private_include/*.h"
)
target_sources(shader_toy_file_system PRIVATE ${HEADERS})

target_include_directories(shader_toy_file_system
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src/private_include
)

find_package(efsw CONFIG REQUIRED)
find_package(readerwriterqueue CONFIG REQUIRED)

target_link_libraries(shader_toy_file_system
    PUBLIC
        shader_toy_system
    PRIVATE
        efsw::efsw
        readerwriterqueue::readerwriterqueue
)

if(WIN32)
    set_target_properties(shader_toy_file_system PROPERTIES
        WINDOWS_EXPORT_ALL_SYMBOLS ON
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/$<CONFIG>
        ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib/$<CONFIG>
    )
endif()

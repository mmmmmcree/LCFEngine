project(image)

file(GLOB_RECURSE HEADERS "include/*.h")
set(SOURCES
    src/Image.cpp
    src/error.cpp
)

add_library(${PROJECT_NAME} SHARED
    ${HEADERS}
    ${SOURCES}
)

if(WIN32)
    set_target_properties(${PROJECT_NAME} PROPERTIES
        WINDOWS_EXPORT_ALL_SYMBOLS ON
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/$<CONFIG>
        ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib/$<CONFIG>
    )    
endif()   

find_package(Stb REQUIRED)
target_include_directories(${PROJECT_NAME}
    PUBLIC
        $<INSTALL_INTERFACE:include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${Stb_INCLUDE_DIR} 
)

target_link_libraries(${PROJECT_NAME}
  PUBLIC
    utilities_core
)

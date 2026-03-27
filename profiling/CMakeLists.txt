project(profiling)

file(GLOB_RECURSE HEADERS "include/*.h")
file(GLOB_RECURSE SOURCES "src/*.cpp")

if(NOT SOURCES)
    add_library(${PROJECT_NAME} INTERFACE)
    target_sources(${PROJECT_NAME} INTERFACE ${HEADERS})
else()
    add_library(${PROJECT_NAME}
        ${HEADERS}   
        ${SOURCES}
    )
endif()
 
if(WIN32 AND SOURCES)
    set_target_properties(${PROJECT_NAME} PROPERTIES
        WINDOWS_EXPORT_ALL_SYMBOLS ON
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/$<CONFIG>
        ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib/$<CONFIG>
    )    
endif()  
  
if(SOURCES)
    include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)
    
    target_include_directories(${PROJECT_NAME}
        PUBLIC
            $<INSTALL_INTERFACE:include>
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/src
    )
else()
    target_include_directories(${PROJECT_NAME}
        INTERFACE
            $<INSTALL_INTERFACE:include>
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    )
endif() 
 
if(SOURCES)
    target_link_libraries(${PROJECT_NAME} 
        PUBLIC
            profiling_config 
    )
else()
    # 对于接口库，使用INTERFACE关键字
    target_link_libraries(${PROJECT_NAME} 
        INTERFACE
            profiling_config 
    )
endif() 
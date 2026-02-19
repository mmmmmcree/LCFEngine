project(containers)

file(GLOB_RECURSE HEADERS "include/*.h")

add_library(${PROJECT_NAME} INTERFACE)
target_sources(${PROJECT_NAME} INTERFACE ${HEADERS})

target_include_directories(${PROJECT_NAME}
    INTERFACE
        $<INSTALL_INTERFACE:include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  )
 
find_package(Boost REQUIRED COMPONENTS graph icl)
find_package(tsl-robin-map CONFIG REQUIRED)

target_link_libraries(${PROJECT_NAME}  
    INTERFACE 
      tsl::robin_map
      Boost::graph
      Boost::icl
)
project(lcf_test_common)

# Header-only shared test fixtures + one TU for static member definitions.
add_library(${PROJECT_NAME} STATIC
    src/throw_on_nth.cpp
)
target_include_directories(${PROJECT_NAME}
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)
target_compile_features(${PROJECT_NAME} PUBLIC cxx_std_23)
target_link_libraries(${PROJECT_NAME}
    PUBLIC
        GTest::gtest
)

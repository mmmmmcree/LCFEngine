add_executable(async_example main.cpp)
target_link_libraries(async_example PRIVATE lcf::async_asio)
set_target_properties(async_example PROPERTIES CXX_EXTENSIONS OFF FOLDER "examples")

add_executable(async_stdexec_example main.cpp)
target_link_libraries(async_stdexec_example PRIVATE lcf::async_stdexec)
set_target_properties(async_stdexec_example PROPERTIES CXX_EXTENSIONS OFF FOLDER "examples/async/stdexec")

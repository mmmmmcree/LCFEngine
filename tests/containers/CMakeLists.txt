# ============================================================
# tests/containers/CMakeLists.txt
#
# Each container has its own subdirectory and its own test executable,
# so individual containers can be built and tested in isolation:
#   ctest -R "containers_flex_array"
#   .\containers_flex_array_unit_tests.exe
#
# To add a new container's tests, drop a subdirectory and append it here:
#   add_subdirectory(ring_buffer)
# ============================================================

add_subdirectory(flex_array)

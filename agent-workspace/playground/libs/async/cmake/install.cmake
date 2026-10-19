include(GNUInstallDirs)
include(CMakePackageConfigHelpers)
set(async_package_asio OFF)
set(async_package_stdexec OFF)
if(TARGET async_asio)
    set(async_package_asio ON)
endif()
if(TARGET async_stdexec)
    set(async_package_stdexec ON)
endif()
configure_package_config_file("${CMAKE_CURRENT_LIST_DIR}/LCFAsyncConfig.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/LCFAsyncConfig.cmake"
    INSTALL_DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/LCFAsync")
write_basic_package_version_file("${CMAKE_CURRENT_BINARY_DIR}/LCFAsyncConfigVersion.cmake"
    VERSION 0.1.0 COMPATIBILITY SameMajorVersion)
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/LCFAsyncConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/LCFAsyncConfigVersion.cmake"
    DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/LCFAsync")

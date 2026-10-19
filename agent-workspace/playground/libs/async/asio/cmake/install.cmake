include(GNUInstallDirs)
install(DIRECTORY include/ DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/lcf_async/asio")
if(LCF_ASYNC_USE_MODULES)
    install(TARGETS async_asio_runtime EXPORT LCFAsyncAsioTargets
        ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}")
    install(TARGETS async_asio EXPORT LCFAsyncAsioTargets
        ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
        FILE_SET CXX_MODULES DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/LCFAsync/modules/asio")
else()
    install(TARGETS async_asio EXPORT LCFAsyncAsioTargets)
endif()
install(EXPORT LCFAsyncAsioTargets NAMESPACE lcf:: DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/LCFAsync"
    CXX_MODULES_DIRECTORY cxx-modules-asio)

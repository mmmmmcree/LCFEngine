include(GNUInstallDirs)
install(DIRECTORY include/ DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/lcf_async/stdexec")
if(LCF_ASYNC_USE_MODULES)
    install(TARGETS async_stdexec EXPORT LCFAsyncStdexecTargets
        ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
        FILE_SET CXX_MODULES DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/LCFAsync/modules/stdexec")
else()
    install(TARGETS async_stdexec EXPORT LCFAsyncStdexecTargets)
endif()
install(EXPORT LCFAsyncStdexecTargets NAMESPACE lcf:: DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/LCFAsync"
    CXX_MODULES_DIRECTORY cxx-modules-stdexec)

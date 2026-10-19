include("${CMAKE_CURRENT_LIST_DIR}/../../../cmake/test_tools.cmake")
function(async_stdexec_test_dependency)
    async_test_dependency(STDEXEC ${ARGV})
endfunction()

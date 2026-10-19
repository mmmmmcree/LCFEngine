execute_process(
    COMMAND "${CMAKE_COMMAND}" --build "${ASYNC_VERIFY_BUILD_DIR}"
        --config "${ASYNC_VERIFY_CONFIG}" --target async_module_private_probe
    RESULT_VARIABLE async_result
    OUTPUT_VARIABLE async_output
    ERROR_VARIABLE async_error)
set(async_diagnostics "${async_output}\n${async_error}")
file(WRITE "${ASYNC_VERIFY_BUILD_DIR}/module-privacy.log" "${async_diagnostics}")
if(async_result EQUAL 0)
    message(FATAL_ERROR "The module unexpectedly exported its internal helper")
endif()
if(NOT async_diagnostics MATCHES "error[^\n]*(details|exception_error)|(details|exception_error)[^\n]*error")
    message(FATAL_ERROR "Compilation failed for an unrelated reason:\n${async_diagnostics}")
endif()
message(STATUS "Internal helper lookup was rejected as expected")

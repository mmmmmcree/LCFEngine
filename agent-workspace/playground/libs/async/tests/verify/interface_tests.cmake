include_guard(GLOBAL)

function(async_add_interface_tests target)
    set(async_interface_sources
        "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/core_test.cpp"
        "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/chain_test.cpp"
        "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/chain_junction_test.cpp"
        "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/error_handling_test.cpp"
        "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/contracts_test.cpp"
        "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/additional_token_test.cpp")
    target_sources(${target} PRIVATE ${async_interface_sources})
    source_group(TREE "${CMAKE_CURRENT_FUNCTION_LIST_DIR}" PREFIX interface FILES ${async_interface_sources})
endfunction()

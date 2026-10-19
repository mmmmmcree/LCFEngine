cmake_minimum_required(VERSION 3.28)
file(MAKE_DIRECTORY "${TEST_DIR}")

# Different orders, absent peers, scientific notation, and mixed time units must
# preserve case identity and compare normalized times. Zero is a measured value.
file(WRITE "${TEST_DIR}/asio.json" [=[{"benchmarks":[
    {"name":"ready,callbacks","real_time":2500,"time_unit":"ns","iterations":10},
    {"name":"zero","real_time":0,"time_unit":"ns","iterations":10},
    {"name":"asio_only","real_time":1e3,"time_unit":"ns","iterations":4},
    {"name":"stdexec_faster","real_time":4,"time_unit":"us","iterations":10},
    {"name":"one_zero","real_time":0,"time_unit":"us","iterations":10}
]}]=])
file(WRITE "${TEST_DIR}/stdexec.json" [=[{"benchmarks":[
    {"name":"zero","real_time":0,"time_unit":"us","iterations":30},
    {"name":"ready,callbacks","real_time":0.005,"time_unit":"ms","iterations":20},
    {"name":"stdexec_only","real_time":1e-6,"time_unit":"s","iterations":5},
    {"name":"stdexec_faster","real_time":2,"time_unit":"us","iterations":20},
    {"name":"one_zero","real_time":1,"time_unit":"us","iterations":20}
]}]=])
set(diagnostics_header "backend,case,size,callbacks,poll_p50_us,poll_p99_us,poll_max_us,max_steps_per_poll,timer_p99_lateness_us\n")
file(WRITE "${TEST_DIR}/asio_diagnostics.csv" "${diagnostics_header}asio,chain,64,65,1,2,3,1,0\nasio,post,256,256,4,5,6,1,0\n")
file(WRITE "${TEST_DIR}/stdexec_diagnostics.csv" "${diagnostics_header}stdexec,post,256,512,7,8,9,1,0\nstdexec,chain,128,642,10,11,12,1,0\n")
set(memory_header "backend,case,size,new_calls,new_requested_bytes,new_peak_live_bytes,new_remaining_bytes\n")
file(WRITE "${TEST_DIR}/asio_memory.csv" "${memory_header}asio,chain,64,5,100,2048,0\nasio,post,256,6,1234567890123456789,4096,0\n")
file(WRITE "${TEST_DIR}/stdexec_memory.csv" "${memory_header}stdexec,post,256,7,200,8192,0\nstdexec,chain,64,8,300,16384,0\n")

function(expect_text filename expected)
    file(READ "${TEST_DIR}/${filename}" actual)
    string(FIND "${actual}" "${expected}" position)
    if(position EQUAL -1)
        message(FATAL_ERROR "${filename} did not contain expected comparison: ${expected}\nActual:\n${actual}")
    endif()
endfunction()

set(report_command "${CMAKE_COMMAND}" "-DRESULTS_DIR=${TEST_DIR}" "-DREPORT_EXECUTABLE=${REPORT_EXECUTABLE}"
    -DREPORT_ONLY=ON -P "${CMAKE_CURRENT_LIST_DIR}/run.cmake")
execute_process(COMMAND ${report_command} RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error)
if(NOT "${result}" STREQUAL "0")
    message(FATAL_ERROR "Valid report fixture failed: ${output}\n${error}")
endif()
expect_text(timing.csv [=["case","asio_us","stdexec_us","winner"]=])
expect_text(timing.csv [=["ready,callbacks","2.500000000","5.000000000","Asio","10","20","ok","ok"]=])
expect_text(timing.csv [=["stdexec_faster","4.000000000","2.000000000","stdexec","10","20","ok","ok"]=])
expect_text(timing.csv [=["zero","0.000000000","0.000000000","tie","10","30","ok","ok"]=])
expect_text(timing.csv [=["one_zero","0.000000000","1.000000000","Asio","10","20","ok","ok"]=])
expect_text(timing.csv [=["asio_only","1.000000000","","n/a","4","","ok","missing"]=])
expect_text(timing.csv [=["stdexec_only","","1.000000000","n/a","","5","missing","ok"]=])
expect_text(diagnostics.csv [=["post","256","256","512","4","7","5","8","6","9"]=])
expect_text(diagnostics.csv [=["chain","64","65","","1","","2","","3",""]=])
expect_text(diagnostics.csv [=["chain","128","","642","","10","","11","","12"]=])
expect_text(memory.csv [=["post","256","6","7","1234567890123456789","200","4096","8192","0","0"]=])
expect_text(summary.md "| ready,callbacks | 2.500 | 5.000 | Asio |")
expect_text(summary.md "| stdexec_faster | 4.000 | 2.000 | stdexec |")
expect_text(summary.md "| zero | 0.000 | 0.000 | tie |")
expect_text(summary.md "| chain/64 | 5 | 8 | 2.000 | 16.000 |")

# Google Benchmark can exit successfully after SkipWithError. Such a record
# must appear as an error and fail the comparison, rather than look faster.
file(READ "${TEST_DIR}/asio.json" report)
string(JSON count LENGTH "${report}" benchmarks)
string(JSON report SET "${report}" benchmarks ${count} [=[{"name":"failed","error_occurred":true,"error_message":"fixture failure"}]=])
file(WRITE "${TEST_DIR}/asio.json" "${report}")
file(READ "${TEST_DIR}/stdexec.json" report)
string(JSON count LENGTH "${report}" benchmarks)
string(JSON report SET "${report}" benchmarks ${count} [=[{"name":"failed","real_time":1,"time_unit":"us","iterations":1}]=])
file(WRITE "${TEST_DIR}/stdexec.json" "${report}")
execute_process(COMMAND ${report_command} RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error)
if("${result}" STREQUAL "0")
    message(FATAL_ERROR "A failed benchmark was reported as successful")
endif()
expect_text(timing.csv [=["failed","","1.000000000","n/a","","1","error","ok"]=])
expect_text(summary.md "| failed | error | 1.000 | n/a |")
message(STATUS "Report fixtures passed: identity, size, units, both winners, ties, missing/zero/error data, and allocation precision")

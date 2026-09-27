# Tests for the onnxcc command line (Part A contract).

# --- Unit tests: drive cli::run() in-process, capturing stdout and stderr ---
add_executable(cli_tests cli_test.cpp)
target_link_libraries(cli_tests PRIVATE onnxcc_cli GTest::gtest_main)
target_compile_options(cli_tests PRIVATE ${ONNXCC_WARNINGS})

include(GoogleTest)
gtest_discover_tests(cli_tests)

# --- End-to-end: run the real onnxcc binary ---
# The unit tests bypass main(); these confirm main() is wired to cli::run() and
# that the real process exit codes and streams match the contract table.
#
#   onnxcc_cli_e2e(<name> EXIT <code> [ARGS ...] [STDOUT <regex>] [STDERR <regex>]
#                  [STDOUT_EMPTY] [STDERR_EMPTY])
function(onnxcc_cli_e2e name)
    cmake_parse_arguments(PARSE_ARGV 1 E2E "STDOUT_EMPTY;STDERR_EMPTY" "EXIT;STDOUT;STDERR" "ARGS")
    # ARGS is passed through as one ;-list, so escape the semicolons.
    string(REPLACE ";" "\\;" args "${E2E_ARGS}")
    set(defs -DEXE=$<TARGET_FILE:onnxcc> "-DARGS=${args}" -DEXPECT_CODE=${E2E_EXIT})
    if(DEFINED E2E_STDOUT)
        list(APPEND defs "-DSTDOUT_MATCH=${E2E_STDOUT}")
    endif()
    if(DEFINED E2E_STDERR)
        list(APPEND defs "-DSTDERR_MATCH=${E2E_STDERR}")
    endif()
    if(E2E_STDOUT_EMPTY)
        list(APPEND defs -DEXPECT_STDOUT_EMPTY=ON)
    endif()
    if(E2E_STDERR_EMPTY)
        list(APPEND defs -DEXPECT_STDERR_EMPTY=ON)
    endif()
    add_test(NAME cli_e2e.${name}
             COMMAND ${CMAKE_COMMAND} ${defs} -P ${CMAKE_CURRENT_SOURCE_DIR}/check_cli.cmake)
endfunction()

# One row per line of the Part A table, in the same order.
onnxcc_cli_e2e(dump_model       EXIT 0 ARGS dump --model path/to/file.onnx
               STDOUT "path/to/file.onnx" STDERR_EMPTY)
onnxcc_cli_e2e(dump_show_graph  EXIT 0 ARGS dump --model f.onnx --show-graph STDERR_EMPTY)
onnxcc_cli_e2e(dump_verbose     EXIT 0 ARGS dump --model f.onnx --verbose STDERR_EMPTY)
onnxcc_cli_e2e(dump_help        EXIT 0 ARGS dump --help
               STDOUT "--model.*--show-graph.*--verbose" STDERR_EMPTY)
onnxcc_cli_e2e(top_level_help   EXIT 0 ARGS --help STDOUT "Usage:.*dump" STDERR_EMPTY)
onnxcc_cli_e2e(top_level_help_short EXIT 0 ARGS -h STDOUT "Usage:.*dump" STDERR_EMPTY)
onnxcc_cli_e2e(dump_no_model    EXIT 2 ARGS dump STDERR "--model" STDOUT_EMPTY)
onnxcc_cli_e2e(bogus            EXIT 2 ARGS bogus STDERR "unknown subcommand 'bogus'" STDOUT_EMPTY)
onnxcc_cli_e2e(no_arguments     EXIT 2 STDERR "Usage:" STDOUT_EMPTY)

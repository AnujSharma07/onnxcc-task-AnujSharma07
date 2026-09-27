# End-to-end check of one CLI invocation against the real onnxcc binary.
#
# Run as a script by ctest (see CMakeLists.txt in this folder):
#   cmake -DEXE=<onnxcc> -DARGS=<a;b;c> -DEXPECT_CODE=<n>
#         [-DSTDOUT_MATCH=<regex>] [-DSTDERR_MATCH=<regex>]
#         [-DEXPECT_STDOUT_EMPTY=ON] [-DEXPECT_STDERR_EMPTY=ON]
#         -P check_cli.cmake
#
# Unlike ctest's WILL_FAIL (which also "passes" on a crash), this checks the
# exact exit code and which stream each message went to. Pure CMake, so it
# works the same on every platform without a shell.

if(NOT DEFINED EXE OR NOT DEFINED EXPECT_CODE)
    message(FATAL_ERROR "check_cli.cmake needs -DEXE and -DEXPECT_CODE")
endif()

execute_process(
    COMMAND ${EXE} ${ARGS}
    RESULT_VARIABLE actual_code
    OUTPUT_VARIABLE actual_out
    ERROR_VARIABLE actual_err
)

set(failures "")

if(NOT actual_code STREQUAL EXPECT_CODE)
    string(APPEND failures "  exit code: expected ${EXPECT_CODE}, got ${actual_code}\n")
endif()
if(DEFINED STDOUT_MATCH AND NOT actual_out MATCHES "${STDOUT_MATCH}")
    string(APPEND failures "  stdout does not match '${STDOUT_MATCH}'\n")
endif()
if(DEFINED STDERR_MATCH AND NOT actual_err MATCHES "${STDERR_MATCH}")
    string(APPEND failures "  stderr does not match '${STDERR_MATCH}'\n")
endif()
if(EXPECT_STDOUT_EMPTY AND NOT actual_out STREQUAL "")
    string(APPEND failures "  stdout should be empty\n")
endif()
if(EXPECT_STDERR_EMPTY AND NOT actual_err STREQUAL "")
    string(APPEND failures "  stderr should be empty\n")
endif()

if(failures)
    message(FATAL_ERROR "onnxcc ${ARGS}\n${failures}"
                        "--- stdout ---\n${actual_out}--- stderr ---\n${actual_err}")
endif()

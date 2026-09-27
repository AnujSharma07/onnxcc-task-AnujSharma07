add_executable(unit_tests
    unit/test_sanity.cpp
)

# link the onnxcc_lib library to the unit_tests executable.
# link the gtest_main library to the unit_tests executable so that the main function for the tests is provided by gtest_main.
target_link_libraries(unit_tests
    PRIVATE
        onnxcc_lib
        GTest::gtest_main
)

# Make the directory path available to the executable as a preprocessor macro because ctest runs from build/ while relative paths are relative to source dir
target_compile_definitions(unit_tests
    PRIVATE
        ONNXCC_FIXTURE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/fixtures"
) 

include(GoogleTest)
# run the exe in background and discover all the tests in the unit_tests executable and register them with CTest. 
gtest_discover_tests(unit_tests)

# Part A command-line contract: unit tests and end-to-end checks of the binary
add_subdirectory(cli)

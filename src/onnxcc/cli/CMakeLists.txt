# onnxcc_cli: command-line parsing and subcommand dispatch.
# A library rather than sources compiled into the executable, so the tests in
# tests/cli can link it and drive cli::run() in-process.
add_library(onnxcc_cli STATIC
    cli.cpp
    dump_command.cpp
)

# onnxcc_lib provides get_version() for `onnxcc --version`, and its PUBLIC
# include dirs give us "onnxcc/..." includes (including the generated version.h).
target_link_libraries(onnxcc_cli PUBLIC onnxcc_lib)

# The vendored cxxopts header. PRIVATE: only dump_command.cpp includes it, so
# nothing that links onnxcc_cli sees it. SYSTEM: warnings inside the
# third-party header are not reported, keeping our build warning-free.
target_include_directories(onnxcc_cli SYSTEM PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../third_party)

target_compile_options(onnxcc_cli PRIVATE ${ONNXCC_WARNINGS})

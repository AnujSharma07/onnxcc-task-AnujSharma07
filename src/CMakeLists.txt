configure_file(onnxcc/version.h.in onnxcc/version.h @ONLY)

add_library(onnxcc_lib STATIC
    onnxcc/version.cpp
)

# Command-line parsing and subcommand dispatch (defines onnxcc_cli)
add_subdirectory(onnxcc/cli)

add_executable(onnxcc onnxcc/main.cpp)

# main.cpp only forwards to onnxcc::cli::run(); onnxcc_cli brings onnxcc_lib with it
target_link_libraries(onnxcc PRIVATE onnxcc_cli)

target_link_libraries(onnxcc_lib
    PRIVATE
        protobuf::libprotobuf
        onnx_proto
)

# Include the current source directory for the onnxcc_lib target
target_include_directories(onnxcc_lib
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_BINARY_DIR}
)

target_compile_options(onnxcc_lib PRIVATE ${ONNXCC_WARNINGS})
target_compile_options(onnxcc PRIVATE ${ONNXCC_WARNINGS})

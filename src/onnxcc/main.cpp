// onnxcc entry point. Deliberately thin: all parsing and dispatch live in
// src/onnxcc/cli/ so they can be unit-tested without spawning a process.
#include <iostream>
#include "onnxcc/cli/cli.hpp"

int main(int argc, char** argv) {
    return onnxcc::cli::run(argc, argv, std::cout, std::cerr);
}

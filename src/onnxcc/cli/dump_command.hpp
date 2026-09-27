#pragma once
#include <iosfwd>

namespace onnxcc::cli {

// Handler for `onnxcc dump`.
//
// Receives the command line with the program name already stripped, so
// argv[0] is "dump" and argv[1..argc) are dump's own options. Every
// subcommand handler has this signature (see the table in cli.cpp).
//
// Options:
//   --model <path>   required; path to the .onnx file
//   --show-graph     boolean flag
//   --verbose        boolean flag
//   -h, --help       print dump's usage to `out` and exit 0
//
// Phase 1 scope: dump validates its arguments but does not open the model yet.
int run_dump(int argc, const char* const* argv, std::ostream& out, std::ostream& err);

} // namespace onnxcc::cli

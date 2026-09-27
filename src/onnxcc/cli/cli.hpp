#pragma once
#include <iosfwd>

namespace onnxcc::cli {

// Parses the full command line, dispatches to the matching subcommand and
// returns the process exit code (see exit_codes.hpp).
//
//   argc, argv  Exactly what main() received; argv[0] is the program name.
//   out         Normal output: help that was asked for, command results.
//   err         Every error message, and the usage shown on failure.
//
// The streams are parameters instead of hard-coded std::cout / std::cerr so
// tests can capture both and check which stream each message went to.
//
// Never throws: any exception becomes a message on `err` and a non-zero code.
int run(int argc, const char* const* argv, std::ostream& out, std::ostream& err);

} // namespace onnxcc::cli

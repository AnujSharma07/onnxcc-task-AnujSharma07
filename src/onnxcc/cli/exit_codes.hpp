#pragma once

// Process exit codes shared by every onnxcc subcommand.
//
// Keeping them in one header means run, compile and benchmark (later phases)
// report failures the same way dump does, and tests compare against a name
// instead of a magic number.

namespace onnxcc::cli {

// Everything worked.
inline constexpr int kExitOk = 0;

// The invocation was valid but the work failed at runtime, e.g. a model file
// that cannot be read once dump actually loads models. Also used for
// unexpected internal errors.
inline constexpr int kExitFailure = 1;

// The invocation itself was wrong: unknown subcommand or option, a missing
// required option, a bad option value. 2 is the conventional "usage error"
// code used by POSIX tools such as grep and diff.
inline constexpr int kExitUsage = 2;

} // namespace onnxcc::cli

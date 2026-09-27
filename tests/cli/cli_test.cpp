// Tests for the Part A command-line contract.
//
// Every test drives onnxcc::cli::run() exactly as main() does, but with
// std::ostringstream in place of std::cout / std::cerr. That lets each test
// check the three things the contract cares about: the exit code, what went
// to stdout, and what went to stderr.
#include <gtest/gtest.h>

#include <initializer_list>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#include "onnxcc/cli/cli.hpp"
#include "onnxcc/cli/exit_codes.hpp"
#include "onnxcc/version.h"

namespace {

using onnxcc::cli::kExitOk;
using onnxcc::cli::kExitUsage;

struct CliResult {
    int exit_code;
    std::string out; // captured stdout
    std::string err; // captured stderr
};

// Runs `onnxcc <args...>` in-process and captures both streams.
CliResult run_cli(std::initializer_list<const char*> args) {
    std::vector<const char*> argv{"onnxcc"};
    argv.insert(argv.end(), args);

    std::ostringstream out;
    std::ostringstream err;
    const int code = onnxcc::cli::run(static_cast<int>(argv.size()), argv.data(), out, err);
    return {code, out.str(), err.str()};
}

// Substring check that prints the whole haystack on failure, so a failing
// test shows the actual message instead of just "false".
::testing::AssertionResult Contains(const std::string& haystack, std::string_view needle) {
    if (haystack.find(needle) != std::string::npos) {
        return ::testing::AssertionSuccess();
    }
    return ::testing::AssertionFailure()
           << "expected to find \"" << needle << "\" in:\n---\n" << haystack << "---";
}

// Shared checks for every rejected invocation: usage-error exit code, nothing
// on stdout, and a message on stderr.
void ExpectUsageError(const CliResult& r) {
    EXPECT_EQ(r.exit_code, kExitUsage);
    EXPECT_TRUE(r.out.empty()) << "stdout should be empty, got:\n" << r.out;
    EXPECT_FALSE(r.err.empty()) << "stderr should explain the error";
}

// ---------------------------------------------------------------------------
// Top level: `onnxcc`, `onnxcc --help`, `onnxcc bogus`
// ---------------------------------------------------------------------------

TEST(CliTopLevel, NoArgumentsFailsWithUsageOnStderr) {
    const CliResult r = run_cli({});
    ExpectUsageError(r);
    EXPECT_TRUE(Contains(r.err, "Usage:"));
    EXPECT_TRUE(Contains(r.err, "dump"));
}

TEST(CliTopLevel, LongAndShortHelpListSubcommandsOnStdout) {
    for (const char* flag : {"--help", "-h"}) {
        SCOPED_TRACE(flag);
        const CliResult r = run_cli({flag});
        EXPECT_EQ(r.exit_code, kExitOk);
        EXPECT_TRUE(r.err.empty()) << "stderr should be empty, got:\n" << r.err;
        EXPECT_TRUE(Contains(r.out, "Usage:"));
        EXPECT_TRUE(Contains(r.out, "dump"));
    }
}

TEST(CliTopLevel, UnknownSubcommandIsNamedOnStderr) {
    const CliResult r = run_cli({"bogus"});
    ExpectUsageError(r);
    EXPECT_TRUE(Contains(r.err, "unknown subcommand 'bogus'"));
}

TEST(CliTopLevel, UnknownTopLevelOptionIsNamedOnStderr) {
    const CliResult r = run_cli({"--frobnicate"});
    ExpectUsageError(r);
    EXPECT_TRUE(Contains(r.err, "unknown option '--frobnicate'"));
}

TEST(CliTopLevel, VersionPrintsProjectVersionOnStdout) {
    const CliResult r = run_cli({"--version"});
    EXPECT_EQ(r.exit_code, kExitOk);
    EXPECT_TRUE(r.err.empty()) << r.err;
    EXPECT_TRUE(Contains(r.out, std::string(onnxcc::get_version())));
}

// ---------------------------------------------------------------------------
// `onnxcc dump`: accepted invocations
// ---------------------------------------------------------------------------

TEST(CliDump, ModelAloneSucceeds) {
    const CliResult r = run_cli({"dump", "--model", "path/to/file.onnx"});
    EXPECT_EQ(r.exit_code, kExitOk);
    EXPECT_TRUE(r.err.empty()) << "stderr should be empty, got:\n" << r.err;
    EXPECT_TRUE(Contains(r.out, "model: path/to/file.onnx"));
}

TEST(CliDump, ModelWithEqualsSyntaxSucceeds) {
    const CliResult r = run_cli({"dump", "--model=f.onnx"});
    EXPECT_EQ(r.exit_code, kExitOk);
    EXPECT_TRUE(Contains(r.out, "model: f.onnx"));
}

TEST(CliDump, ShowGraphFlagIsAcceptedAndChangesOutput) {
    const CliResult plain = run_cli({"dump", "--model", "f.onnx"});
    const CliResult graph = run_cli({"dump", "--model", "f.onnx", "--show-graph"});
    EXPECT_EQ(graph.exit_code, kExitOk);
    EXPECT_TRUE(graph.err.empty()) << graph.err;
    EXPECT_TRUE(Contains(graph.out, "graph:"));
    EXPECT_FALSE(Contains(plain.out, "graph:"));
}

TEST(CliDump, VerboseFlagIsAcceptedAndChangesOutput) {
    const CliResult quiet = run_cli({"dump", "--model", "f.onnx"});
    const CliResult verbose = run_cli({"dump", "--model", "f.onnx", "--verbose"});
    EXPECT_EQ(verbose.exit_code, kExitOk);
    EXPECT_TRUE(verbose.err.empty()) << verbose.err;
    EXPECT_TRUE(Contains(verbose.out, "verbose: yes"));
    EXPECT_FALSE(Contains(quiet.out, "verbose: yes"));
}

TEST(CliDump, OptionOrderDoesNotMatter) {
    const CliResult r = run_cli({"dump", "--verbose", "--show-graph", "--model", "f.onnx"});
    EXPECT_EQ(r.exit_code, kExitOk);
    EXPECT_TRUE(Contains(r.out, "model: f.onnx"));
    EXPECT_TRUE(Contains(r.out, "show-graph: yes"));
}

// A boolean flag must not swallow the next word as its value. If it did,
// `b.onnx` below would be silently eaten; instead it must be reported.
TEST(CliDump, BooleanFlagsDoNotConsumeTheNextArgument) {
    const CliResult r = run_cli({"dump", "--model", "a.onnx", "--show-graph", "b.onnx"});
    ExpectUsageError(r);
    EXPECT_TRUE(Contains(r.err, "unexpected argument 'b.onnx'"));
}

TEST(CliDump, LongAndShortHelpListAllThreeOptionsOnStdout) {
    for (const char* flag : {"--help", "-h"}) {
        SCOPED_TRACE(flag);
        const CliResult r = run_cli({"dump", flag}); // no --model needed for help
        EXPECT_EQ(r.exit_code, kExitOk);
        EXPECT_TRUE(r.err.empty()) << r.err;
        EXPECT_TRUE(Contains(r.out, "--model"));
        EXPECT_TRUE(Contains(r.out, "--show-graph"));
        EXPECT_TRUE(Contains(r.out, "--verbose"));
    }
}

// ---------------------------------------------------------------------------
// `onnxcc dump`: rejected invocations. All must exit with kExitUsage, write
// nothing to stdout, and explain the problem on stderr.
// ---------------------------------------------------------------------------

TEST(CliDump, MissingModelFailsOnStderr) {
    const CliResult r = run_cli({"dump"});
    ExpectUsageError(r);
    EXPECT_TRUE(Contains(r.err, "missing required option --model"));
}

TEST(CliDump, FlagsWithoutModelStillFail) {
    const CliResult r = run_cli({"dump", "--show-graph", "--verbose"});
    ExpectUsageError(r);
    EXPECT_TRUE(Contains(r.err, "missing required option --model"));
}

TEST(CliDump, ModelWithoutValueFails) {
    const CliResult r = run_cli({"dump", "--model"});
    ExpectUsageError(r);
    EXPECT_TRUE(Contains(r.err, "model"));
}

// cxxopts would take "--verbose" as the model path; validate() rejects that.
TEST(CliDump, ModelFollowedByAnotherFlagIsRejected) {
    const CliResult r = run_cli({"dump", "--model", "--verbose"});
    ExpectUsageError(r);
    EXPECT_TRUE(Contains(r.err, "--model needs a path"));
}

// Without this check cxxopts would keep "b.onnx" and silently drop "a.onnx".
TEST(CliDump, RepeatedModelIsRejected) {
    const CliResult r = run_cli({"dump", "--model", "a.onnx", "--model", "b.onnx"});
    ExpectUsageError(r);
    EXPECT_TRUE(Contains(r.err, "more than once"));
}

TEST(CliDump, EmptyModelPathIsRejected) {
    const CliResult r = run_cli({"dump", "--model", ""});
    ExpectUsageError(r);
    EXPECT_TRUE(Contains(r.err, "non-empty"));
}

TEST(CliDump, UnknownOptionIsNamedOnStderr) {
    const CliResult r = run_cli({"dump", "--model", "f.onnx", "--frobnicate"});
    ExpectUsageError(r);
    EXPECT_TRUE(Contains(r.err, "frobnicate"));
}

TEST(CliDump, BadBooleanValueIsRejected) {
    const CliResult r = run_cli({"dump", "--model", "f.onnx", "--verbose=maybe"});
    ExpectUsageError(r);
    EXPECT_TRUE(Contains(r.err, "maybe"));
}

TEST(CliDump, BarePathWithoutModelFlagIsRejected) {
    const CliResult r = run_cli({"dump", "f.onnx"});
    ExpectUsageError(r);
    EXPECT_TRUE(Contains(r.err, "unexpected argument 'f.onnx'"));
}

} // namespace

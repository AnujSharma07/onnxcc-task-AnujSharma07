#include "onnxcc/cli/dump_command.hpp"

#include <cxxopts.hpp>
#include <optional>
#include <ostream>
#include <string>
#include <string_view>

#include "onnxcc/cli/exit_codes.hpp"

namespace onnxcc::cli {
namespace {

    // The validated result of parsing dump's options. Parsing and doing the work
    // are kept apart so that when dump starts loading models, only
    // execute_dump() has to change.
    struct DumpOptions {
        std::string model_path;
        bool show_graph = false;
        bool verbose = false;
    };

    void add_dump_options(cxxopts::Options& options) {
        // Replaces cxxopts' default "[OPTION...]" in the usage line.
        options.custom_help("--model <path> [--show-graph] [--verbose]");

        // `help` has no value(), so cxxopts treats it as a boolean flag.
        // The two value<bool> flags default to false and never consume the next
        // token: `--show-graph` alone means true.
        // clang-format off
        options.add_options()
            ("model", "Path to the .onnx model file (required)",
             cxxopts::value<std::string>(), "<path>")
            ("show-graph", "Print the model's graph",
             cxxopts::value<bool>()->default_value("false"))
            ("verbose", "Print extra detail about what dump is doing",
             cxxopts::value<bool>()->default_value("false"))
            ("h,help", "Show this help and exit");
        // clang-format on
    }

    // Prints a usage error on `err` and returns the matching exit code, so each
    // failure path below reads as `return usage_error(err, "...");`.
    int usage_error(std::ostream& err, std::string_view message) {
        err << "onnxcc dump: error: " << message << '\n'
            << "Run 'onnxcc dump --help' for usage.\n";
        return kExitUsage;
    }

    // Checks cxxopts cannot do for us. Returns an error message, or
    // std::nullopt when the options are acceptable.
    std::optional<std::string> validate(const DumpOptions& opts) {
        if (opts.model_path.empty()) {
            return "--model needs a non-empty path";
        }
        // cxxopts takes whatever token follows --model as its value, so
        // `dump --model --verbose` would set the model path to "--verbose".
        // Reject that instead of silently accepting it.
        if (opts.model_path.starts_with('-')) {
            return "--model needs a path, but got '" + opts.model_path +
                   "' (did you forget the path?)";
        }
        // Deliberately no existence check: the Phase 1 contract says dump
        // validates its arguments but does not open the file. That check
        // belongs with the code that reads it, where a failure is kExitFailure
        // (a runtime error), not kExitUsage.
        return std::nullopt;
    }

    // The actual work. For now dump only reports what it was asked to do; the
    // ONNX loader will replace the placeholder lines.
    int execute_dump(const DumpOptions& opts, std::ostream& out) {
        out << "model: " << opts.model_path << '\n';
        if (opts.verbose) {
            out << "show-graph: " << (opts.show_graph ? "yes" : "no") << '\n'
                << "verbose: yes\n"
                << "note: model loading is not implemented yet; arguments were validated only\n";
        }
        if (opts.show_graph) {
            out << "graph: not available yet (requires the ONNX loader)\n";
        }
        return kExitOk;
    }

} // namespace

int run_dump(int argc, const char* const* argv, std::ostream& out, std::ostream& err) {
    cxxopts::Options parser("onnxcc dump", "Inspect an ONNX model.");
    add_dump_options(parser);

    try {
        const cxxopts::ParseResult result = parser.parse(argc, argv);

        // Help first, so `onnxcc dump --help` works without --model.
        if (result.count("help") > 0) {
            out << parser.help();
            return kExitOk;
        }

        // Words cxxopts did not consume, e.g. `dump model.onnx` (forgot --model)
        // or `dump --model a.onnx --show-graph b.onnx`.
        if (!result.unmatched().empty()) {
            return usage_error(err, "unexpected argument '" + result.unmatched().front() + "'");
        }

        // cxxopts has no "required option" feature, so check it ourselves.
        if (result.count("model") == 0) {
            return usage_error(err, "missing required option --model <path>");
        }

        DumpOptions opts;
        opts.model_path = result["model"].as<std::string>();
        opts.show_graph = result["show-graph"].as<bool>();
        opts.verbose = result["verbose"].as<bool>();

        if (const auto problem = validate(opts)) {
            return usage_error(err, *problem);
        }
        return execute_dump(opts, out);
    } catch (const cxxopts::exceptions::exception& e) {
        // Unknown option, --model with no value, a bad boolean value, ...
        // cxxopts' own message already names the offending option.
        return usage_error(err, e.what());
    }
}

} // namespace onnxcc::cli

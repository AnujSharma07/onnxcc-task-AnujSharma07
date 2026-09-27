#include "onnxcc/cli/cli.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <exception>
#include <ostream>
#include <string>
#include <string_view>

#include "onnxcc/cli/dump_command.hpp"
#include "onnxcc/cli/exit_codes.hpp"
#include "onnxcc/version.h"

namespace onnxcc::cli {
namespace {

    // Signature shared by all subcommand handlers. argv[0] is the subcommand name.
    using Handler = int (*)(int argc, const char* const* argv, std::ostream& out,
                            std::ostream& err);

    struct Subcommand {
        std::string_view name;    // what the user types, e.g. "dump"
        std::string_view summary; // one line shown in `onnxcc --help`
        Handler handler;
    };

    // The single list of subcommands. Dispatch and the top-level usage text are
    // both driven from this table, so they cannot drift apart.
    //
    // Adding run, compile or benchmark later:
    //   1. write <name>_command.{hpp,cpp} exposing a Handler-shaped function,
    //   2. add one row here,
    //   3. add the .cpp to onnxcc_cli in src/onnxcc/cli/CMakeLists.txt.
    constexpr std::array kSubcommands{
        Subcommand{"dump", "Inspect an ONNX model", &run_dump},
    };

    // cxxopts parses the options *of* a subcommand but has no notion of
    // subcommands, so the first word (argv[1]) is matched by hand here.
    const Subcommand* find_subcommand(std::string_view name) {
        const auto it = std::find_if(kSubcommands.begin(), kSubcommands.end(),
                                     [name](const Subcommand& sub) { return sub.name == name; });
        return it == kSubcommands.end() ? nullptr : &*it;
    }

    bool is_help_flag(std::string_view arg) { return arg == "-h" || arg == "--help"; }

    bool is_version_flag(std::string_view arg) { return arg == "-V" || arg == "--version"; }

    void print_usage(std::ostream& os) {
        // Width of the longest subcommand name, so the summaries line up.
        std::size_t width = 0;
        for (const Subcommand& sub : kSubcommands) {
            width = std::max(width, sub.name.size());
        }

        os << "Usage: onnxcc <subcommand> [options]\n"
           << "\n"
           << "Subcommands:\n";
        for (const Subcommand& sub : kSubcommands) {
            os << "  " << sub.name << std::string(width - sub.name.size() + 2, ' ')
               << sub.summary << '\n';
        }
        os << "\n"
           << "Options:\n"
           << "  -h, --help     Show this help and exit\n"
           << "  -V, --version  Show the onnxcc version and exit\n"
           << "\n"
           << "Run 'onnxcc <subcommand> --help' for the options of a subcommand.\n";
    }

} // namespace

int run(int argc, const char* const* argv, std::ostream& out, std::ostream& err) {
    try {
        // `onnxcc` with nothing after it is an error, so usage goes to stderr.
        if (argc < 2) {
            err << "onnxcc: error: no subcommand given\n\n";
            print_usage(err);
            return kExitUsage;
        }

        const std::string_view first = argv[1];

        // `onnxcc --help` / `onnxcc -h`: the user asked for it, so stdout and exit 0.
        if (is_help_flag(first)) {
            print_usage(out);
            return kExitOk;
        }

        if (is_version_flag(first)) {
            out << "onnxcc " << get_version() << " (" << get_version_codename() << ")\n";
            return kExitOk;
        }

        if (const Subcommand* sub = find_subcommand(first)) {
            // Drop the program name. The handler then sees argv[0] == "dump",
            // which is the shape cxxopts expects (it skips argv[0] when parsing).
            return sub->handler(argc - 1, argv + 1, out, err);
        }

        // Name the bad token so the user sees exactly what was rejected.
        if (first.starts_with('-')) {
            err << "onnxcc: error: unknown option '" << first << "'\n";
        } else {
            err << "onnxcc: error: unknown subcommand '" << first << "'\n";
        }
        err << "Run 'onnxcc --help' to list the available subcommands.\n";
        return kExitUsage;
    } catch (const std::exception& e) {
        // Handlers deal with their own parse errors; reaching this means a bug or
        // something like std::bad_alloc. Report it instead of calling terminate().
        err << "onnxcc: internal error: " << e.what() << '\n';
        return kExitFailure;
    } catch (...) {
        err << "onnxcc: internal error: unknown exception\n";
        return kExitFailure;
    }
}

} // namespace onnxcc::cli

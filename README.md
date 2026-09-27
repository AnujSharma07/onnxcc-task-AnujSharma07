# onnxcc

A small C++20 compiler and inference engine for ONNX models. It reads an
`.onnx` file, builds its own IR, allocates tensors and runs the graph node by
node. The Phase 1 goal is output that matches ONNX Runtime to within 1e-4.

This repository currently holds the selection-task work on top of the starter:
the command line (Part A), the test-fixture generator (Part B) and the tests
(Part C).

## Build and test

Requires CMake >= 3.26, a C++20 compiler, and network access on the first
configure (protobuf, ONNX and GoogleTest come in through FetchContent; the
first build compiles protobuf and takes several minutes).

```bash
rm -rf build && cmake -S . -B build && cmake --build build
ctest --test-dir build --output-on-failure
```

Our own targets build with `-Wall -Wextra -Wpedantic` (`/W4` on MSVC) and no
warnings.

## Command line (Part A)

```
onnxcc <subcommand> [options]

onnxcc dump --model <path> [--show-graph] [--verbose]
onnxcc --help | -h
onnxcc dump --help | -h
onnxcc --version | -V
```

| Invocation                               | Exit | Output                                      |
|------------------------------------------|------|---------------------------------------------|
| `onnxcc dump --model path/to/file.onnx`  | 0    | stdout                                      |
| `onnxcc dump --model f.onnx --show-graph`| 0    | stdout                                      |
| `onnxcc dump --model f.onnx --verbose`   | 0    | stdout                                      |
| `onnxcc dump --help`                     | 0    | dump usage with its three options on stdout |
| `onnxcc --help`                          | 0    | top-level usage listing subcommands, stdout |
| `onnxcc dump` (no `--model`)             | 2    | error on stderr                             |
| `onnxcc bogus`                           | 2    | `unknown subcommand 'bogus'` on stderr      |
| `onnxcc` (no arguments)                  | 2    | usage on stderr                             |

Exit codes live in `src/onnxcc/cli/exit_codes.hpp`: `0` success, `1` runtime
failure (reserved for when dump actually reads models), `2` usage error.

Also rejected with exit 2 and a message on stderr: an unknown option, `--model`
with no value, `--model --verbose` (a flag taken as the path), an empty path,
a bare path without `--model` (`dump f.onnx`), and bad boolean values
(`--verbose=maybe`).

**Phase 1 scope:** `dump` validates its arguments but does not open the file,
as the task specifies. `--show-graph` and `--verbose` are parsed and change the
output, but only print placeholders until the ONNX loader exists.

### Layout

```
src/onnxcc/main.cpp              thin: forwards argc/argv + std::cout/std::cerr to cli::run
src/onnxcc/cli/cli.{hpp,cpp}     top-level parsing, subcommand table, dispatch
src/onnxcc/cli/dump_command.*    dump's options (cxxopts), validation, execution
src/onnxcc/cli/exit_codes.hpp    shared exit codes
src/onnxcc/third_party/          vendored cxxopts v3.3.1 (single header, see its README)
```

**Adding a subcommand** (`run`, `compile`, `benchmark`): write
`<name>_command.{hpp,cpp}` with a function of the same shape as `run_dump`,
add one row to `kSubcommands` in `cli.cpp`, and add the `.cpp` to
`src/onnxcc/cli/CMakeLists.txt`. Dispatch and `onnxcc --help` both come from
that table.

`cli::run` takes the output and error streams as parameters, so tests capture
them and check which stream each message went to.

## Test fixtures (Part B)

```bash
python3 -m pip install numpy onnx          # onnxruntime optional
python3 scripts/generate_test_models.py    # writes into tests/fixtures/
```

| File                  | Contents                                                        |
|-----------------------|-----------------------------------------------------------------|
| `mlp_4_8_2.onnx`      | 4 -> 8 -> 2 MLP, ReLU after each layer, opset 13, IR version 7   |
| `mlp_4_8_2_input.bin` | one `(1, 4)` float32 input, raw little-endian, 16 bytes          |

The graph is 6 nodes (`MatMul`, `Add`, `Relu` twice each) and 4 initializers
(`W1`, `b1`, `W2`, `b2`). It is built with `onnx.helper` rather than exported
from PyTorch, whose exporter fuses `nn.Linear` into `Gemm`, which Phase 1 does
not implement.

The script is idempotent: fixed seed (42), pinned IR version, deterministic
serialization, no timestamps, and files are only rewritten if their bytes
change. It runs `onnx.checker` (full check), asserts the op types, node and
initializer counts, opset and the 16-byte input size, and, if `onnxruntime` is
installed, checks ORT's output against a NumPy reference.

The generated `.onnx` / `.bin` files are gitignored. Commit the script, not
its output.

## Tests (Part C)

`ctest` runs two layers:

- **`cli_tests`** (GoogleTest, `tests/cli/cli_test.cpp`): calls `cli::run`
  in-process with `std::ostringstream`s and checks exit code, stdout and stderr
  for every row of the table above, plus the edge cases listed under Part A.
- **`cli_e2e.*`** (`tests/cli/CMakeLists.txt` + `check_cli.cmake`): runs the
  real `onnxcc` binary for each table row and checks the exact exit code and
  which stream the text went to. This covers `main()` wiring, which the unit
  tests bypass. It is plain CMake, so it needs no shell and works the same on
  every platform.

The starter's `unit_tests` (sanity checks) are unchanged.

## Not done yet

- `dump` does not load or print the model; `--show-graph` / `--verbose` print
  placeholders.
- The fixture is not yet consumed by a C++ test. That happens once the loader
  and the MatMul/Add/Relu kernels exist.

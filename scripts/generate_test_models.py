#!/usr/bin/env python3
"""Generate the ONNX test fixtures used by onnxcc's Phase 1 tests.

Outputs (default directory: <repo>/tests/fixtures/, the ONNXCC_FIXTURE_DIR that
tests/CMakeLists.txt passes to the unit tests; *.onnx and *.bin are gitignored):

    mlp_4_8_2.onnx         4 -> 8 -> 2 MLP, ReLU after each layer, opset 13
    mlp_4_8_2_input.bin    one (1, 4) float32 input, little-endian, 16 bytes

Graph:

    input [1,4]
      -> MatMul(W1 [4,8]) -> Add(b1 [8]) -> Relu      layer 1
      -> MatMul(W2 [8,2]) -> Add(b2 [2]) -> Relu      layer 2
      -> output [1,2]

    Nodes:        6  (MatMul x2, Add x2, Relu x2)
    Initializers: 4  (W1, b1, W2, b2)

Why onnx.helper and not torch.onnx.export: PyTorch's exporter fuses nn.Linear
into a single Gemm node, and Phase 1 does not implement Gemm. Building the graph
by hand guarantees only MatMul, Add and Relu appear, and the script asserts it.

Idempotent: a fixed seed, fixed names and metadata, no timestamps, and
deterministic protobuf serialization, so every run produces byte-identical
files. Files whose content has not changed are not rewritten.

Usage:
    python3 scripts/generate_test_models.py [--out-dir DIR]

Requires: numpy, onnx. If onnxruntime is installed, the model is also run
through it and compared against the NumPy reference.
"""

from __future__ import annotations

import argparse
import hashlib
import sys
from pathlib import Path

import numpy as np
import onnx
from onnx import TensorProto, helper, numpy_helper

SEED = 42
OPSET = 13
# IR version 7 is the one that shipped alongside opset 13. Pinning it (instead
# of taking the installed onnx package's newest IR) keeps the file loadable by
# older runtimes and stops the output changing when onnx is upgraded.
IR_VERSION = 7

IN_FEATURES, HIDDEN, OUT_FEATURES = 4, 8, 2
INPUT_SHAPE = (1, IN_FEATURES)

ALLOWED_OPS = {"MatMul", "Add", "Relu"}
EXPECTED_NODE_COUNT = 6
EXPECTED_INITIALIZER_COUNT = 4
EXPECTED_INPUT_BYTES = 16  # 1 * 4 values * 4 bytes per float32

REPO_ROOT = Path(__file__).resolve().parent.parent
DEFAULT_OUT_DIR = REPO_ROOT / "tests" / "fixtures"
MODEL_FILE = "mlp_4_8_2.onnx"
INPUT_FILE = "mlp_4_8_2_input.bin"


def make_parameters(rng: np.random.Generator) -> dict[str, np.ndarray]:
    """Weights, biases and the input, all float32, drawn from one seeded RNG.

    The draw order is fixed; changing it changes every value in the fixture.
    """
    return {
        "W1": (rng.standard_normal((IN_FEATURES, HIDDEN)) * 0.5).astype(np.float32),
        "b1": (rng.standard_normal(HIDDEN) * 0.1).astype(np.float32),
        "W2": (rng.standard_normal((HIDDEN, OUT_FEATURES)) * 0.5).astype(np.float32),
        "b2": (rng.standard_normal(OUT_FEATURES) * 0.1).astype(np.float32),
        "input": rng.standard_normal(INPUT_SHAPE).astype(np.float32),
    }


def reference_forward(p: dict[str, np.ndarray]) -> tuple[np.ndarray, np.ndarray]:
    """NumPy version of the graph. Returns (layer-1 pre-activation, output)."""
    pre1 = p["input"] @ p["W1"] + p["b1"]
    hidden = np.maximum(pre1, 0.0)
    output = np.maximum(hidden @ p["W2"] + p["b2"], 0.0)
    return pre1, output


def check_fixture_is_useful(pre1: np.ndarray, output: np.ndarray) -> None:
    """Guard against a degenerate fixture that would hide bugs.

    If Relu clamped nothing, a missing Relu would still pass. If the output
    were all zeros, almost any broken MatMul would still pass.
    """
    if not (pre1 < 0).any() or not (pre1 > 0).any():
        sys.exit("fixture check failed: layer-1 Relu should clamp some units and keep others")
    if not (output > 0).all():
        sys.exit("fixture check failed: every output value should be non-zero")


def build_model(p: dict[str, np.ndarray]) -> onnx.ModelProto:
    """Build the 6-node MLP graph with onnx.helper."""
    initializers = [numpy_helper.from_array(p[name], name) for name in ("W1", "b1", "W2", "b2")]

    # One node per op. Intermediate tensor names match the node names, which
    # makes the graph easy to read in `onnxcc dump --show-graph` later.
    nodes = [
        helper.make_node("MatMul", ["input", "W1"], ["fc1_matmul"], name="fc1_matmul"),
        helper.make_node("Add", ["fc1_matmul", "b1"], ["fc1_add"], name="fc1_add"),
        helper.make_node("Relu", ["fc1_add"], ["fc1_relu"], name="fc1_relu"),
        helper.make_node("MatMul", ["fc1_relu", "W2"], ["fc2_matmul"], name="fc2_matmul"),
        helper.make_node("Add", ["fc2_matmul", "b2"], ["fc2_add"], name="fc2_add"),
        helper.make_node("Relu", ["fc2_add"], ["output"], name="fc2_relu"),
    ]

    graph = helper.make_graph(
        nodes,
        "mlp_4_8_2",
        inputs=[helper.make_tensor_value_info("input", TensorProto.FLOAT, list(INPUT_SHAPE))],
        outputs=[helper.make_tensor_value_info("output", TensorProto.FLOAT, [1, OUT_FEATURES])],
        initializer=initializers,
    )
    model = helper.make_model(
        graph,
        producer_name="onnxcc-generate_test_models",
        producer_version="1",
        opset_imports=[helper.make_opsetid("", OPSET)],
    )
    model.ir_version = IR_VERSION
    return model


def write_if_changed(path: Path, data: bytes) -> bool:
    """Write `data` to `path` unless the file already holds exactly that.

    Returns True if the file was written. Skipping identical writes keeps the
    modification time stable, so build tools do not see a spurious change.
    """
    if path.exists() and path.read_bytes() == data:
        return False
    path.write_bytes(data)
    return True


def display(path: Path) -> str:
    """Path relative to the repo when possible, so output has no absolute paths."""
    try:
        return path.resolve().relative_to(REPO_ROOT).as_posix()
    except ValueError:
        return path.as_posix()


def verify_written_files(model_path: Path, input_path: Path) -> onnx.ModelProto:
    """Re-read the files from disk and check the whole Part B contract."""
    onnx.checker.check_model(str(model_path), full_check=True)
    model = onnx.load(str(model_path))

    op_types = [node.op_type for node in model.graph.node]
    unexpected = set(op_types) - ALLOWED_OPS
    if unexpected:
        sys.exit(f"unexpected op types {sorted(unexpected)}; only {sorted(ALLOWED_OPS)} allowed")
    if len(op_types) != EXPECTED_NODE_COUNT:
        sys.exit(f"expected {EXPECTED_NODE_COUNT} nodes, found {len(op_types)}")
    if len(model.graph.initializer) != EXPECTED_INITIALIZER_COUNT:
        sys.exit(f"expected {EXPECTED_INITIALIZER_COUNT} initializers, "
                 f"found {len(model.graph.initializer)}")

    opsets = {op.domain: op.version for op in model.opset_import}
    if opsets.get("", opsets.get("ai.onnx")) != OPSET:
        sys.exit(f"expected default-domain opset {OPSET}, found {opsets}")

    size = input_path.stat().st_size
    if size != EXPECTED_INPUT_BYTES:
        sys.exit(f"{display(input_path)} is {size} bytes, expected {EXPECTED_INPUT_BYTES}")
    return model


def cross_check_with_onnxruntime(model_path: Path, x: np.ndarray, expected: np.ndarray) -> None:
    """Optional: run the model in ONNX Runtime and compare with NumPy."""
    try:
        import onnxruntime as ort  # noqa: PLC0415 (optional dependency)
    except ImportError:
        print("onnxruntime: not installed, cross-check skipped")
        return
    session = ort.InferenceSession(str(model_path), providers=["CPUExecutionProvider"])
    (actual,) = session.run(None, {"input": x})
    if not np.allclose(actual, expected, atol=1e-6):
        sys.exit(f"onnxruntime output {actual} does not match NumPy reference {expected}")
    print(f"onnxruntime: output matches NumPy reference (max abs diff "
          f"{float(np.max(np.abs(actual - expected))):.2e})")


def sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--out-dir", type=Path, default=DEFAULT_OUT_DIR,
                        help="where to write the fixtures (default: <repo>/tests/fixtures)")
    args = parser.parse_args()

    params = make_parameters(np.random.default_rng(SEED))
    pre1, expected = reference_forward(params)
    check_fixture_is_useful(pre1, expected)

    model = build_model(params)
    onnx.checker.check_model(model, full_check=True)  # fail before touching disk

    args.out_dir.mkdir(parents=True, exist_ok=True)
    model_path = args.out_dir / MODEL_FILE
    input_path = args.out_dir / INPUT_FILE

    # deterministic=True fixes protobuf map ordering, one ingredient of idempotence.
    model_bytes = model.SerializeToString(deterministic=True)
    # '<f4' = little-endian float32 regardless of the host's byte order.
    input_bytes = params["input"].astype("<f4").tobytes()

    for path, data in ((model_path, model_bytes), (input_path, input_bytes)):
        state = "wrote" if write_if_changed(path, data) else "unchanged"
        print(f"{state:9}  {display(path)}  sha256={sha256(path)[:16]}")

    written = verify_written_files(model_path, input_path)
    print("onnx.checker: passed (full_check)")
    print(f"opset: {OPSET}, ir_version: {written.ir_version}")
    print(f"op types: {[n.op_type for n in written.graph.node]}")
    print(f"nodes: {len(written.graph.node)}, initializers: {len(written.graph.initializer)}")
    print(f"input bytes: {input_path.stat().st_size}")
    print(f"input values:     {params['input'].ravel().tolist()}")
    print(f"expected output:  {expected.ravel().tolist()}")

    cross_check_with_onnxruntime(model_path, params["input"], expected)
    return 0


if __name__ == "__main__":
    sys.exit(main())

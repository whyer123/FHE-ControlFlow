# FHE Control Flow Prototype

A research prototype demonstrating homomorphic encryption (FHE) for secure comparison operations on encrypted data, enabling conditional logic (e.g., `while` loops) based on encrypted inputs without revealing the full data.

## Current Controlled Reveal Prototype

The current demo implements the first controlled-reveal step:

```text
g(c_x,c_b) = FHE.Dec(hsk, FHE.Eval([x <= b], c_x, c_b))
```

The predicate is evaluated as a bit-level comparator using `AND`, `XOR`, and `NOT`, then only the resulting predicate bit is decrypted. The demo reveals `[a <= b]` and does not decrypt `a`, `b`, or the encrypted loop state.

The demo calls the predicate through an `EncryptedPredicateEvaluator` interface so the current controlled-reveal prototype can later be replaced by an offline garbled-circuit artifact.

The loop demo keeps `x`, `a`, and `b` encrypted. Each round reveals only `[x <= b]`, then updates the encrypted state with `one' = Enc(1)` supplied by the client.

`Circuit_g` is exported as a garbling-ready Boolean circuit with stable wire ids, gate ids, input wires, output wires, and a demo OpenFHE LWE-like decryption arithmetic subcircuit for `b - <a,hsk> mod q`. The latest demo dump is written to `artifacts/circuit_g_demo.txt`.

In `MOCK_OPENFHE` mode, the Docker `gc_mock` target now uses an EMP-toolkit half-gates garbled-circuit artifact on every loop iteration. The in-repo minimal GC backend remains available as a fallback when `USE_EMP_GC` is not set.

## OpenFHE EvalBinGate Circuit Dump

The repo now includes a first OpenFHE-shaped Boolean-circuit expansion for
`EvalBinGate`. It expands LWE ciphertext field inputs, OpenFHE's additive
pre-bootstrap step, fixed demo `hsk` selected-label constants, and a named
bootstrap placeholder that emits an LWE ciphertext.

```bash
./build_mock.sh --evalbingate-circuit
```

This writes:

```text
artifacts/openfhe_evalbingate_and_demo.txt
artifacts/openfhe_evalbingate_or_demo.txt
artifacts/openfhe_evalbingate_xor_demo.txt
artifacts/openfhe_evalbingate_xnor_demo.txt
```

Current limitation: `openfhe_bootstrap_placeholder` is not the real OpenFHE
blind-rotation/RGSW bootstrapping core yet. It is deliberately named in the
circuit dump so this step cannot be mistaken for the final production circuit.

## Fixed-Bound Runtime Demo (Transitional Mock)

The fixed-runtime path models the first offline demo target:

```text
Client/setup side, precomputed:
  a' = Enc(a)
  fixed_b material
  one' = Enc(1)
  GC_f = Garble(Dec_hsk(Eval([x <= fixed_b], x')))

Evaluator runtime:
  load a', one', evaluation material, fixed GC_f
  do not load hpk
  do not load hsk
  do not accept free b' input
```

For fast local validation:

```bash
./build_mock.sh --fixed-setup
./build_mock.sh --fixed-runtime
```

The fixed circuit dump is written to `artifacts/fixed_bound_circuit_g_demo.txt`. Its public inputs are only `x_i`; fixed `b` and decryption constants are represented as selected-label constants. The evaluator runtime loads `artifacts/fixed_bound_circuit_shape.bin` and `artifacts/fixed_bound_gc_artifact.bin`, not `hpk` or `hsk`.

This path is transitional and not the final contract. The target interface is
still `GC_f(x', b')`; only setup/key/GC generation are precomputed.

To verify the real OpenFHE runtime material boundary:

```bash
cmake -S . -B build-local -DCMAKE_BUILD_TYPE=Release
cmake --build build-local --target prepare_openfhe_constants evaluator_runtime_material_check --parallel 2
./build-local/prepare_openfhe_constants demo_keys/openfhe_binfhe_demo_keypair
./build-local/evaluator_runtime_material_check demo_keys/openfhe_binfhe_demo_keypair
```

`evaluator_runtime_material_check` loads only context/evaluation keys plus prepared ciphertext bits for `a'` and integer `one'`, then performs one encrypted state update with OpenFHE `EvalBinGate`.

See `gc.md` for a detailed Chinese walkthrough of the controlled reveal design and demo flow.

See `report.md` for a Chinese explanation of the demo loop output.

See `demo_keys/fixed_demo_material.md` for the fixed demo hsk/GC material and generated OpenFHE BinFHE `hpk/hsk` files.

## Getting Started

### Using Docker (自動執行)

```bash
docker-compose up --build
```
這會在背景自動編譯並執行展示程式，結束後自動跳出。

快速跑 mock GC demo：

```bash
docker-compose run --rm gc_mock
```

`gc_mock` installs EMP-toolkit in the image and runs the fixed setup/runtime path with `USE_EMP_GC=1`, so the output should include:

```text
Evaluator runtime input policy: fixed GC_f(x')
Evaluator runtime did not load hpk or hsk
```

跑完整 OpenFHE demo：

```bash
docker-compose run --rm fhe_control_flow
```

### 直接進入 Docker 內部互動執行 (Interactive Mode)

如果您希望進入容器內部自己編譯、跑指令或是除錯，請執行以下指令：

```bash
# 啟動並進入容器內的 bash
docker-compose run --rm -it fhe_control_flow bash

# 進入容器後，手動編譯並執行
mkdir -p build && cd build
cmake ..
make -j$(nproc)
./demo_loop
```

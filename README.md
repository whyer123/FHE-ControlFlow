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

## OpenFHE Controlled-Reveal Logic

Before lowering the whole expression into a Boolean circuit, the repo includes
one lowering-ready OpenFHE reference logic file for the exact target:

```text
Dec_hsk(OpenFHE.Eval([x <= b], x', b'))
```

The single-file source is:

```text
src/gc/openfhe_controlled_reveal_reference.cpp
```

This file has no `main`, no includes, no OpenFHE runtime types, no filesystem
paths, no serialization, and no printing. `FIXED_HSK`, local LWE ciphertext
structs, the encrypted comparator, and the controlled decrypt are all in one
place so the next lowering step can replace them with Boolean circuit
wires/constants. It intentionally does not include the loop `add one` helper.

The lowering source uses STD128-shaped LWE dimensions and modulus constants
with a fixed in-file ternary `FIXED_HSK`. The verification target does not link
OpenFHE or use the repo-local demo key directory:

```bash
cmake -S . -B build-local -DCMAKE_BUILD_TYPE=Release
cmake --build build-local --target openfhe_controlled_reveal_reference_test --parallel 2
./build-local/openfhe_controlled_reveal_reference_test
```

The logic file does not expose a reusable `Dec_hsk(c')` API; decryption is
tied to the predicate ciphertext produced by `EvalLessOrEqualPredicate`.

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

## OpenFHE LWE Integer GC Runtime Demo

The current v2 path now uses OpenFHE-generated integer LWE ciphertext fields
and the generated secret key material during setup:

```text
Setup side:
  export_openfhe_lwe_int_material -> a'=Enc(3), b'=Enc(7), one'=Enc(1), hsk.s_mod_q
  setup_openfhe_lwe_int_gc_material -> circuit shape + GC artifact + sanitized runtime material

Evaluator runtime:
  openfhe_lwe_int_runtime_demo
  loads only a', b', one', circuit shape, and GC artifact
  does not load hpk or hsk
```

Run the formal EMP-backed STD128 demo:

```bash
./run_formal_demo.sh
```

`./run_demo.sh` is kept as a short alias for the same formal demo.

The formal demo saves terminal output and inspectable runtime artifacts under:

```text
artifacts/formal_emp_showcase/
```

Useful files after a run:

```text
artifacts/formal_emp_showcase/formal_emp_demo_output.txt
artifacts/formal_emp_showcase/runtime_output.txt
artifacts/formal_emp_showcase/full_setup_material.txt
artifacts/formal_emp_showcase/runtime/openfhe_lwe_int_circuit_g_demo.txt
artifacts/formal_emp_showcase/runtime/openfhe_lwe_int_gc_artifact.bin
artifacts/formal_emp_showcase/runtime/openfhe_lwe_int_runtime_material.txt
```

The expected runtime evidence is:

```text
GC backend: EMP half-gates
Evaluator runtime did not load hpk or hsk
Predicate sequence: 1,1,1,1,1,0
Encrypted loop iterations executed: 5
```

For a faster local regression without EMP, run:

```bash
bash tests/test_openfhe_lwe_int_runtime_demo.sh
```

That test also runs `audit_openfhe_lwe_int_runtime_boundary`, which checks that
the evaluator runtime directory does not contain setup-only fields such as
`hsk.s_raw`, `hsk.s_mod_q`, `manual_dec`, clear plaintext fields, or `hpk/hsk`
key-file references. It also verifies that the serialized circuit shape keeps
only secret wire ids, not secret values.

Expected runtime predicate sequence for the fixed demo values is:

```text
Predicate sequence: 1,1,1,1,1,0
Encrypted loop iterations executed: 5
```

The exporter also accepts explicit demo plaintexts, so the same circuit/GC path
can be tested against multiple OpenFHE-generated ciphertext pairs:

```bash
./build-local/export_openfhe_lwe_int_material \
  demo_keys/openfhe_binfhe_demo_keypair \
  /tmp/v2_lwe_integer_material_8_7.txt \
  8 7 1
```

`bash tests/test_openfhe_lwe_int_multiple_values.sh` exercises several such
pairs, including false cases like `8 <= 7`.
`bash tests/test_openfhe_lwe_int_runtime_multiple_values.sh` additionally
drives the full evaluator runtime loop over multiple OpenFHE-generated
materials, checking predicate sequences such as `1,1,1,0` for `1..3` and `0`
for already-false starts like `8..7`.

The setup-side material format is versioned as
`openfhe_lwe_int_setup_material_v1`. Runtime material is separately versioned
as `openfhe_lwe_int_runtime_material_v1`, so evaluator-side tooling rejects
ad-hoc or stale material files instead of silently parsing them.
The setup tool also rejects wraparound-unsafe demo loops: this first runtime
version requires `one'=Enc(1)`, and when `a <= b` it requires `b + 1 < p`.
It also decrypts the setup-side encrypted increment chain and rejects
noise-unsafe material where repeated `x' = x' + one'` no longer decodes as
`a, a+1, ..., b+1`. In that case client setup should re-export fresh
ciphertexts before producing the runtime GC artifact.

This demo implements:

```text
GC_f(x', b') = GC{ [Dec_hsk(x') <= Dec_hsk(b')] }
```

The repository's checked-in key directory is still OpenFHE `TOY` material for
fast local tests. To generate the same 4-bit integer demo over a normal
OpenFHE parameter set, use the keygen tool with an explicit paramset:

```bash
./build-local/generate_openfhe_keypair /tmp/openfhe_std128_keys STD128
./build-local/export_openfhe_lwe_int_material \
  /tmp/openfhe_std128_keys \
  /tmp/v2_lwe_integer_material_std128.txt
```

The plaintext domain remains `p=16` for the first loop demo; the selected
paramset controls the OpenFHE LWE key/ciphertext parameters. The old
fixed-bound mock path remains for comparison.

The checked opt-in STD128 path can be run with:

```bash
RUN_STD128_OPENFHE_TEST=1 \
  bash tests/test_openfhe_lwe_int_std128_runtime_demo.sh
```

On the current local OpenFHE build this produced `n=556`, `q=2048`, `12254`
runtime public input wires, a roughly `571MB` temporary key directory, and a
roughly `174MB` runtime directory (`110MB` GC artifact). The test retries
material export if setup rejects a noisy increment chain.

To run the same v2 runtime path through `build_mock.sh`, first generate a
material file with OpenFHE, then pass it to the GC-only build:

```bash
./build-local/export_openfhe_lwe_int_material \
  demo_keys/openfhe_binfhe_demo_keypair \
  /tmp/v2_lwe_integer_material.txt

./build_mock.sh --v2-openfhe-runtime-demo \
  /tmp/v2_lwe_integer_material.txt \
  /tmp/openfhe_lwe_int_runtime
```

Inside the `gc_mock` Docker target the same command uses `USE_EMP_GC=1`, so it
runs the v2 setup/runtime with the EMP half-gates backend instead of the local
minimal fallback.

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

For the v2 OpenFHE integer LWE runtime path, generate the material file first
on an OpenFHE-enabled environment into the mounted repo, then run:

```bash
./build-local/export_openfhe_lwe_int_material \
  demo_keys/openfhe_binfhe_demo_keypair \
  artifacts/v2_lwe_integer_material.docker.tmp.txt
```

```bash
docker-compose run --rm gc_mock \
  ./build_mock.sh --v2-openfhe-runtime-demo \
  artifacts/v2_lwe_integer_material.docker.tmp.txt \
  artifacts/openfhe_lwe_int_runtime_docker_tmp
```

Alternatively, use the combined OpenFHE + EMP service directly. It generates
STD128 OpenFHE LWE integer material and then runs the EMP-backed v2 runtime in
the same container:

```bash
docker-compose run --rm openfhe_emp_v2_runtime
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

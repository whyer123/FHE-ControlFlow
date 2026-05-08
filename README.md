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

See `gc.md` for a detailed Chinese walkthrough of the controlled reveal design and demo flow.

See `report.md` for a Chinese explanation of the demo loop output.

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

`gc_mock` installs EMP-toolkit in the image and runs `build_mock.sh` with `USE_EMP_GC=1`, so the predicate path should print:

```text
Evaluator predicate path: EMP half-gates GC artifact
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

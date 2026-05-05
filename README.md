# FHE Control Flow Prototype

A research prototype demonstrating homomorphic encryption (FHE) for secure comparison operations on encrypted data, enabling conditional logic (e.g., `while` loops) based on encrypted inputs without revealing the full data.

## Current Controlled Reveal Prototype

The current demo implements the first controlled-reveal step:

```text
g(c_x,c_b) = FHE.Dec(hsk, FHE.Eval([x <= b], c_x, c_b))
```

The predicate is evaluated as a bit-level comparator using `AND`, `XOR`, and `NOT`, then only the resulting predicate bit is decrypted. The demo reveals `[a <= b]` and does not decrypt `a`, `b`, or the encrypted loop state.

The demo calls the predicate through an `EncryptedPredicateEvaluator` interface so the current controlled-reveal prototype can later be replaced by an offline garbled-circuit artifact.

The loop demo keeps `x`, `a`, and `b` encrypted. Each round reveals only `[x <= b]`, then updates the encrypted state with a bit-level homomorphic increment.

`Circuit_g` is exported as a garbling-ready Boolean circuit with stable wire ids, gate ids, input wires, output wires, and a demo OpenFHE LWE-like decryption arithmetic subcircuit for `b - <a,hsk> mod q`.

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

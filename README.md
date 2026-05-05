# FHE Control Flow Prototype

A research prototype demonstrating homomorphic encryption (FHE) for secure comparison operations on encrypted data, enabling conditional logic (e.g., `while` loops) based on encrypted inputs without revealing the full data.

## Current Controlled Reveal Prototype

The current demo implements the first controlled-reveal step:

```text
g(c_x,c_b) = FHE.Dec(hsk, FHE.Eval([x <= b], c_x, c_b))
```

The predicate is evaluated as a bit-level comparator using `AND`, `XOR`, and `NOT`, then only the resulting predicate bit is decrypted. The demo reveals `[a <= b]` and does not decrypt `a`, `b`, or the encrypted loop state.

## Current ABE2 Prototype

The current implementation models the GKP13-style offline control-flow idea with an `ABESelector` that advances a state token through:

- `CHECK_CONDITION`
- `LOOP_BODY`
- `HALT`

This means the evaluator can keep executing the loop without calling back to the client for each branch decision. The demo now runs from an encrypted `a'` down to an encrypted `b'`, and it does not print the final plaintext state. The current limitation is that the branch extraction is still simulated locally with the FHE secret key, because the project does not yet contain a real ABE backend or garbled-circuit/ABE integration layer.

## Getting Started

### Using Docker (自動執行)

```bash
docker-compose up --build
```
這會在背景自動編譯並執行展示程式，結束後自動跳出。

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

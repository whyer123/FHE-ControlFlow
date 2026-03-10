# FHE Control Flow Prototype

A research prototype demonstrating homomorphic encryption (FHE) for secure comparison operations on encrypted data, enabling conditional logic (e.g., `while` loops) based on encrypted inputs without revealing the full data.

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

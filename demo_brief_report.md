# Demo 簡短說明報告

## 1. 如何跑 demo

### 快速測試版

快速版使用 repo 內已準備的 OpenFHE TOY key material，適合確認流程，但測試腳本主要是給開發驗證用：

```bash
bash tests/test_openfhe_lwe_int_runtime_demo.sh
```

### STD128 + EMP 現場展示版

若要展示目前主線 demo，直接跑：

```bash
./run_emp_showcase.sh
```

它會在 Docker 內完成：

```text
OpenFHE STD128 key generation
OpenFHE LWE integer material export
EMP half-gates GC setup
evaluator runtime loop
```

展示時重點看這幾行輸出：

```text
GC backend: EMP half-gates
Evaluator runtime did not load hpk or hsk
Predicate sequence: 1,1,1,1,1,0
Encrypted loop iterations executed: 5
```

這條路線的展示 material 都在容器 `/tmp` 內產生，結束後不會把大型 key、GC artifact 或 poster 產物留在 repo。

### STD128 測試版

若要在本機跑正常 OpenFHE `STD128` 參數的測試：

```bash
RUN_STD128_OPENFHE_TEST=1 \
  bash tests/test_openfhe_lwe_int_std128_runtime_demo.sh
```

`STD128` 版本會產生大型臨時 material，測試結束會自動清掉。先前實測約需要 `571MB` key material 和 `174MB` runtime artifact。這個指令適合證明正常參數跑得通，但不適合現場展示檔案，因為跑完會清掉產物。

### Local fallback 檔案檢查版

如果你需要保留 GC 前 Boolean circuit、runtime material、GC artifact 檔案給教授看，可以跑 local fallback 腳本：

```bash
./run_std128_showcase.sh
```

它會輸出到：

```text
artifacts/std128_showcase/
```

注意：這個 local 腳本預設使用本機 active backend；若本機沒有 EMP headers/lib，會走 `in-repo minimal fallback`。因此正式 EMP 展示請用 `./run_emp_showcase.sh`。

檔案檢查時可看這幾行輸出：

```text
GC backend: in-repo minimal fallback
Evaluator runtime did not load hpk or hsk
Predicate sequence: 1,1,1,1,1,0
Encrypted loop iterations executed: 5
```

如果要看 GC 前的 Boolean circuit：

```bash
head -20 artifacts/std128_showcase/runtime/openfhe_lwe_int_circuit_g_demo.txt
```

如果要看已 garble 的 GC artifact 大小：

```bash
ls -lh artifacts/std128_showcase/runtime/openfhe_lwe_int_gc_artifact.bin
```

如果要看 evaluator runtime 拿到什麼：

```bash
cat artifacts/std128_showcase/runtime/openfhe_lwe_int_runtime_material.txt
```

如果要看 setup 端完整 material：

```bash
less artifacts/std128_showcase/full_setup_material.txt
```

注意：`full_setup_material.txt` 內含 `hsk.s_mod_q`，只能給 client/setup 看，不能交給 evaluator。

展示完可以刪掉整個展示資料夾：

```bash
rm -rf artifacts/std128_showcase
```

## 2. Demo 流程內容介紹

Demo 的目標是讓 evaluator 在沒有 `hpk/hsk` 的情況下跑加密 loop。

Client/setup 先用 OpenFHE 產生：

```text
a' = Enc_hpk(3)
b' = Enc_hpk(7)
one' = Enc_hpk(1)
```

接著 setup 產生 GC：

```text
GC_f(x', b') = GC{ [Dec_hsk(x') <= Dec_hsk(b')] }
```

意思是 GC 內部固定藏入 `hsk`，但 evaluator 只能得到比較結果 `[x <= b]`，不能直接得到 `x`、`b` 或 `hsk`。

Evaluator runtime 執行：

```text
x' = a'
while true:
    cond = GC_f(x', b')
    if cond == 0: stop
    x' = x' + one'
```

對 demo 參數 `a=3, b=7`，每輪語意是：

```text
3 <= 7 -> 1
4 <= 7 -> 1
5 <= 7 -> 1
6 <= 7 -> 1
7 <= 7 -> 1
8 <= 7 -> 0
```

所以輸出 `1,1,1,1,1,0`，實際執行 5 次 loop。

## 3. 用到的檔案

主要程式：

```text
tools/export_openfhe_lwe_int_material.cpp
```

用 OpenFHE `hpk/hsk` 產生 `a'`、`b'`、`one'`，並匯出 LWE ciphertext fields 與 setup 用的 `hsk.s_mod_q`。

```text
tools/setup_openfhe_lwe_int_gc_material.cpp
```

用 `hsk.s_mod_q` 建立 Boolean circuit、GC artifact，以及 evaluator runtime 可用的 sanitized material。

```text
examples/openfhe_lwe_int_runtime_demo.cpp
```

Evaluator runtime，只載入 `a'`、`b'`、`one'`、circuit shape、GC artifact，不載入 `hpk/hsk`。

EMP Docker 展示入口：

```text
run_emp_showcase.sh
docker-compose.yml
Dockerfile
```

`run_emp_showcase.sh` 會呼叫 `openfhe_emp_v2_runtime` service，在 Docker 內使用 `USE_EMP_GC=ON` 和 EMP half-gates backend 跑完整 runtime demo。

Local fallback 重要產物：

```text
artifacts/std128_showcase/full_setup_material.txt
```

setup-side 完整 material，含 `hsk.s_mod_q`，不可交給 evaluator。

```text
artifacts/std128_showcase/runtime/openfhe_lwe_int_circuit_g_demo.txt
```

GC 前的 Boolean circuit 文字版，可直接查看 gate list。

```text
artifacts/std128_showcase/runtime/openfhe_lwe_int_gc_artifact.bin
```

已 garble 的 GC artifact，binary 格式，不適合人眼閱讀，但 runtime 會載入它。

```text
artifacts/std128_showcase/runtime/openfhe_lwe_int_runtime_material.txt
```

Evaluator runtime material，只含 ciphertext fields，不含 `hpk/hsk`。

補充文件：

```text
gc.md
report.md
todo.md
```

`gc.md` 解釋 GC 與 Boolean circuit；`report.md` 解釋 demo loop 輸出；`todo.md` 記錄已完成與仍待處理事項。

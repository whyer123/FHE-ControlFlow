# OpenFHE EvalBinGate + Dec_hsk 轉 Boolean Circuit 大小估算報告

日期：2026-05-16

## 1. 結論摘要

若把以下整體函數完整編譯成 Boolean circuit 並做 Garbled Circuit：

```text
Dec_hsk(OpenFHE.EvalBinGate(gate, c1, c2))
```

即使只使用目前專案中的 OpenFHE `TOY` 參數，預估也會達到：

```text
單一 EvalBinGate + Dec_hsk：
約 1B ~ 5B 個 AND-equivalent Boolean gates
約 30GB ~ 160GB garbled table artifact

完整 4-bit [x <= b] predicate + Dec_hsk：
目前 comparator 約需要 13 次 EvalBinGate
約 13B ~ 65B 個 AND-equivalent Boolean gates
約 400GB ~ 2TB garbled table artifact
```

這裡的 `B` 是 billion，也就是十億級 gates。這不是因為 `Dec_hsk` 很大，而是因為 OpenFHE 的 `EvalBinGate` 內部包含 bootstrapping、CGGI accumulator、polynomial arithmetic、NTT、key switching 等大量 word-level arithmetic；轉成 Boolean circuit 後，每個 word-level operation 都會被展成 bit-level gates。

因此，完整把 `OpenFHE.EvalBinGate + Dec_hsk` 包進 GC，在理論上可描述，但作為可執行 demo 並不實際。

## 2. 目前估算使用的 OpenFHE 參數

本估算使用目前專案固定 demo material 的 OpenFHE BinFHE `TOY` 參數，而不是 production-security `STD128`。

目前 standalone lowering source 使用的主要參數：

```text
small LWE dimension n = 64
small LWE modulus q = 512
ring dimension N = 512
ring modulus Q = 134215681
Q bit-width 約 27 bits
plaintext modulus p = 4
CGGI gadget base Bg = 512
CGGI gadget digits = 3
CGGI external-product digits = (3 - 1) * 2 = 4
key-switch base Bks = 25
key-switch digits = 6
```

目前固定 OpenFHE evaluation material 大小：

```text
eval_refresh_key.bin：約 4.0MB
eval_switch_key.bin：約 39MB
binary total：約 43MB

匯出成 plain C++ integer arrays 後：約 70MB
```

匯出後的 word 數：

```text
refresh key words = 524,288
switch key A words = 4,915,200
switch key B words = 76,800
total eval-key words = 5,516,288
```

這些 evaluation keys 是 public evaluation material，不是 `hpk`，也不是 `hsk`。

## 3. 為什麼 CPU 可以跑，但 Boolean circuit 會爆炸

一般 CPU 執行 OpenFHE 時，`uint64_t` 加法、乘法、模運算、記憶體索引、NTT butterfly 都是由硬體和高度最佳化 library 直接處理。

但是 GC 不能直接使用 CPU 指令。GC 需要把整個計算重新描述成 Boolean circuit：

```text
32-bit add  -> 多個 full adders
27-bit multiply -> partial products + adder tree + modular reduction
array select -> comparator / mux network
polynomial multiply -> 大量 coefficient operations
NTT -> 多層 butterfly，每層包含 modular add/sub/mul
```

所以「電腦中本來也是邏輯運算」這件事是對的，但 CPU 的邏輯電路已經存在於硬體中；GC 則是要把這些硬體行為重新展開成可 garble 的 gates。

## 4. 各部分大小來源

### 4.1 Dec_hsk 本身很小

`Dec_hsk` 的核心是：

```text
phase = b - <a, hsk> mod q
decoded = floor(p * (phase + q/(2p)) / q)
```

在目前 TOY 參數下：

```text
n = 64
q = 512
q bit-width = 9
hsk coefficients are ternary: -1, 0, 1
```

因此 `Dec_hsk` 只需要：

```text
64 次 small modular add/sub
少量 ternary coefficient selection
最後 rounding / bit extraction
```

估算：

```text
Dec_hsk 約 1K ~ 10K gates
```

相較於十億級的 `EvalBinGate`，這部分幾乎可以忽略。

### 4.2 LWE ciphertext add 也很小

OpenFHE `EvalBinGate` 前段會先做 LWE 加法：

```text
AND/OR 類 gate: c1 + c2
XOR/XNOR 類 gate: 2 * (c1 + c2)
```

目前 small LWE ciphertext 有：

```text
64 個 a coefficients + 1 個 b body
每個 coefficient 約 9 bits
```

估算：

```text
LWE add / double 約 1K ~ 10K gates
```

這也不是主要問題。

### 4.3 BootstrapGateCore 是主要爆炸來源

OpenFHE `EvalBinGate` 的核心成本在：

```text
BootstrapGateCore
```

其主要流程是：

```text
1. 根據 gate constant 建立 accumulator LUT
2. 對 input ciphertext 的 a-vector 做 CGGI accumulator update
3. 每個 update 需要 signed digit decomposition
4. 每個 update 需要 RGSW external product
5. external product 涉及 polynomial / NTT-domain multiplication
```

目前 TOY 參數下：

```text
n = 64
N = 512
external-product digits = 4
每次 AddToAccCGGI 約需 16 組 polynomial/eval-key multiplication
總 AddToAccCGGI 次數 = n = 64
```

若使用 OpenFHE 的 NTT/evaluation-domain 表示，每次 polynomial multiplication 主要變成 `N=512` 個 27-bit modular multiplications。

粗估 modular multiplication 數量：

```text
64 LWE coefficients
* 16 polynomial multiplications per coefficient
* 512 pointwise ring coefficients
= 524,288 次 27-bit modular multiplication
```

除此之外，signed digit decomposition 和 NTT conversion 也會增加同量級的 modular add/sub/mul。

保守估算：

```text
BootstrapGateCore 約 1B ~ 5B gates
```

這是整個 circuit 最大的部分。

### 4.4 SwitchCTtoqn 也很大，但通常小於 BootstrapGateCore

OpenFHE bootstrapping 後會得到大維度 ciphertext，還要透過 key switching 回到 small LWE ciphertext：

```text
SwitchCTtoqn
= ModSwitch(Q -> qKS)
 + KeySwitch
 + ModSwitch(qKS -> q)
```

目前 key-switch material 維度：

```text
N = 512
baseKS = 25
digitsKS = 6
small n = 64
```

KeySwitch 的核心 loop 約是：

```text
for i in 0..511:
  for digit in 0..5:
    a0 = digit decomposition of ct.a[i] in base 25
    subtract switch_key_A[i][a0][digit][0..63]
    subtract switch_key_B[i][a0][digit]
```

GC 裡如果 `a0` 是 runtime-dependent，就不能直接用 CPU array indexing，需要 Boolean mux / selector network。

估算：

```text
512 * 6 = 3,072 次 digit-dependent selection
每次 selection 需要選出約 64 個 27-bit A coefficients + 1 個 27-bit B coefficient
```

估算：

```text
SwitchCTtoqn 約 100M ~ 500M gates
```

它很大，但通常仍小於 `BootstrapGateCore`。

## 5. 單一 EvalBinGate + Dec_hsk 的總估算

把各部分加總：

```text
LWE add / double:       ~1K - 10K gates
accumulator init:       ~10K - 100K gates
BootstrapGateCore:      ~1B - 5B gates
SwitchCTtoqn:           ~100M - 500M gates
Dec_hsk:                ~1K - 10K gates
```

主導項：

```text
BootstrapGateCore
```

總估算：

```text
單一 EvalBinGate + Dec_hsk
約 1B ~ 5B AND-equivalent gates
```

若使用 half-gates GC，常見估算是一個 AND gate 約需要 2 個 ciphertext blocks；若每個 block 16 bytes，約為：

```text
1 AND gate 約 32 bytes garbled table
```

因此：

```text
1B gates -> 約 32GB
5B gates -> 約 160GB
```

尚未計入所有 wire labels、metadata、I/O labels、serialization overhead。

## 6. 完整 4-bit [x <= b] predicate 的估算

目前 comparator logic 大致是：

```text
greater_0 = (!b0) AND x0

for i = 1..3:
  generate  = (!bi) AND xi
  xor_bits  = bi XOR xi
  equal     = NOT(xor_bits)
  propagate = equal AND greater
  greater   = generate XOR propagate

result = NOT(greater)
```

其中 `NOT` 不需要 bootstrapping，但 `AND` / `XOR` 都會呼叫 `EvalBinGate`。

目前 4-bit comparator 約需要：

```text
initial AND: 1
for each of 3 higher bits: AND + XOR + AND + XOR = 4
total EvalBinGate calls = 1 + 3 * 4 = 13
```

所以完整 predicate：

```text
13 * (單一 EvalBinGate + Dec_hsk)
```

估算：

```text
13B ~ 65B gates
garbled table 約 400GB ~ 2TB
```

## 7. eval keys 大和 circuit 大是兩件事

需要區分兩個問題。

第一，eval keys 本身很大：

```text
binary eval keys 約 43MB
plain C++ arrays 約 70MB
```

這是資料大小。

第二，Boolean circuit 很大：

```text
約 1B ~ 5B gates per EvalBinGate
```

這是計算展開後的 gate 數。

也就是說，70MB 不是最嚴重的問題。更嚴重的是：這些 eval-key words 會被大量 modular multiplication、NTT、external product、key switching 使用；一旦把這些操作全部 bit-level 展開，circuit size 會遠超 70MB。

## 8. 對 demo 設計的影響

如果研究目標是完整性，可以繼續推：

```text
GC 包住 OpenFHE.EvalBinGate + Dec_hsk
```

但這會導致非常大的 circuit 和 GC artifact，不適合作為一般 demo。

比較務實的 demo 路線是：

```text
OpenFHE.Eval([x <= b], x', b') 在 GC 外執行
GC 只包 Dec_hsk(c_pred')
```

這會讓 GC 變小很多，因為 `Dec_hsk` 本身很小。

但是這條路線需要額外處理安全模型：

```text
避免 evaluator 把 GC_dec 當成任意 decrypt oracle
```

可能的限制方式包括：

```text
固定 predicate circuit
固定 b' 或固定 allowed b' material
不給 hpk
只給 evaluation keys
限制 GC input 必須是該 predicate 的 output ciphertext shape
```

## 9. 建議結論

目前可以向教授說明：

```text
我們確認了完整 GC(OpenFHE.EvalBinGate + Dec_hsk) 的理論方向，
也已經開始把 OpenFHE gate constants、accumulator init、CGGI primitives
寫成 lowering-ready C++。

但根據目前 TOY 參數估算，單一 EvalBinGate + Dec_hsk 可能已達
十億級 Boolean gates，GC artifact 可能是數十 GB 到百 GB。

完整 4-bit comparator 需要約 13 次 EvalBinGate，artifact 可能達
數百 GB 到 TB 等級。

因此，若目標是可執行 demo，應考慮把 OpenFHE.Eval 留在 GC 外，
GC 只做受限 Dec_hsk，並透過固定 predicate / 固定 material / 不給 hpk
來限制 decrypt oracle 風險。
```

## 10. 參考來源

- OpenFHE `BootstrapGateCore` source listing: <https://openfhe-development.readthedocs.io/en/latest/api/program_listing_file_binfhe_lib_binfhe-base-scheme.cpp.html>
- OpenFHE CGGI accumulator source listing: <https://openfhe-development.readthedocs.io/en/latest/api/program_listing_file_binfhe_lib_rgsw-acc-cggi.cpp.html>
- OpenFHE LWE key switching source listing: <https://openfhe-development.readthedocs.io/en/latest/api/program_listing_file_binfhe_lib_lwe-pke.cpp.html>
- Project reference source: `src/gc/openfhe_controlled_reveal_reference.cpp`
- Project eval-key exporter: `tools/export_openfhe_eval_key_material.cpp`

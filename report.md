# Demo Loop Report

這份 report 專門說明目前 `examples/demo_loop.cpp` 的 demo 在做什麼，以及執行輸出的每一段代表什麼。

## Demo 目標

demo 展示的是一個 controlled reveal 的 encrypted loop：

```text
x' = a'
while true:
    cond = GC_f(x', b')
    if cond == 0:
        stop
    x' = x' + 1'
```

其中：

```text
GC_f(x', b') = Dec(hsk, FHE.Eval([x <= b], x', b'))
```

demo 允許 evaluator 看到：

```text
[x <= b]
loop iteration count
runtime
```

demo 不直接揭露：

```text
a
b
x
hsk
```

目前 mock demo 固定使用：

```text
a = 3
b = 7
bit_length = 4
hsk = [1, 0, 1, 1]
LWE mask a = [3, 5, 6, 1]
q = 16
```

`a`、`b` 只是在 demo 內用來產生 encrypted endpoints：

```text
a' = Enc(a)
b' = Enc(b)
```

之後 loop 操作的是 encrypted state `x'`。

## Demo 流程

### 1. 建立 FHE context

輸出：

```text
Initializing Context and Keys...
Creating bootstrapping keys, this might take a moment...
Bootstrapping keys generated.
```

意義：

程式建立 FHE context、secret key、bootstrapping key。mock mode 下這些是快速模擬；完整 OpenFHE mode 下會使用 OpenFHE BinFHE context。

### 2. 加密 loop 邊界

輸出：

```text
Encrypting loop endpoints a' and b' (bit_length = 4)
```

意義：

demo 把起點和終點加密成 bit-level ciphertext vector：

```text
a' = [a_0', a_1', a_2', a_3']
b' = [b_0', b_1', b_2', b_3']
```

後續 comparator 和 increment 都在 bit-level ciphertext 上運算。

### 3. 建立 GC artifact

輸出：

```text
Evaluator predicate path: EMP half-gates GC artifact
GC artifact name: g(c_x,c_b)=Dec(Eval([x<=b],c_x,c_b))
GC artifact gate count: 338
Public input label pairs: 8
Hardcoded secret/constant labels: 36
Circuit_g constant wires: 36
EMP half-gates AND count: 159
EMP transcript blocks: 321
```

意義：

`Evaluator predicate path: EMP half-gates GC artifact` 表示 mock demo 的 loop predicate 已經走 EMP-toolkit half-gates backend，不是直接呼叫 C++ controlled reveal decrypt。

`GC artifact name` 表示目前被包進 GC 的函數：

```text
g(c_x,c_b)=Dec(Eval([x<=b],c_x,c_b))
```

`GC artifact gate count: 338` 表示 `Circuit_g` 目前在 4-bit demo 參數下有 338 個 Boolean gates。這個數字會隨 bit length、LWE dimension、modulus bit width、decode logic 變動。

`Public input label pairs: 8` 來自 4-bit `x'` 和 4-bit `b'`：

```text
x_0, b_0, x_1, b_1, x_2, b_2, x_3, b_3
```

每個 public input wire 有 0/1 兩個 labels。依照目前研究假設，evaluator 可以持有這些 labels，因此可以離線查詢不同 ciphertext 對應的 predicate。

`Hardcoded secret/constant labels: 36` 包含：

```text
hardcoded_openfhe_lwe_hsk_0..3
openfhe_lwe_a_i_bit_j
adder constants
```

這些不是 public 0/1 label pairs，而是 garbler 寫進 GC artifact 的 selected labels。尤其 `hardcoded_openfhe_lwe_hsk_0..3` 代表固定 hsk：

```text
hsk = [1, 0, 1, 1]
```

evaluator 可以使用這些 labels evaluate GC，但不會直接知道它們對應的 bit 值。

`Circuit_g constant wires` 表示 Boolean circuit 內有多少條 constant wires。現在 decryption 子電路包含固定 hsk、固定 LWE mask coefficients、加法器 carry constants，因此數量會比早期 mock 版本多。

`EMP half-gates AND count` 表示 EMP backend 實際 garble 的 AND gates 數量。`EMP transcript blocks` 是 half-gates garbling 後 evaluator 需要讀取的 transcript block 數量。

### 4. Circuit_g gate list

輸出會列出：

```text
g0: w8(not_b_0) <- NOT(w1(b_0))
...
g337: w381(predicate_bit) <- OUTPUT(w368(openfhe_lwe_phase_sum_2))
```

意義：

這是 garbling-ready Boolean circuit 描述。每一個 gate 都有：

```text
gate id
output wire id
output wire name
gate type
input wire ids
input wire names
```

前半段是 comparator：

```text
predicate_msg = [x <= b]
```

後半段是 OpenFHE LWE-like decryption arithmetic：

```text
b = encoded_msg + <a,hsk> mod q
phase = b - <a,hsk> mod q
predicate_bit = phase[2]
```

因此 circuit 形狀已經是：

```text
Eval(f) -> Dec -> output predicate
```

### 5. EMP GC evaluation

輸出：

```text
EMP GC evaluated [a <= b] = 1
```

意義：

這一步在 `MOCK_OPENFHE` 模式下，把 `a'`、`b'` 的 mock ciphertext bits 編成 input labels，然後用 EMP `HalfGateEva` evaluate `Circuit_g`。

它驗證：

```text
a = 3
b = 7
3 <= 7
```

所以輸出 predicate `1`。

這一步的重點是確認 GC artifact 的 label flow 可以跑通：

```text
input bits -> input labels -> EMP half-gates transcript -> output label -> predicate bit
```

### 6. Encrypted evaluator loop

輸出：

```text
GC_f(x', b') revealed predicate [x <= b] = 1
GC_f(x', b') revealed predicate [x <= b] = 1
GC_f(x', b') revealed predicate [x <= b] = 1
GC_f(x', b') revealed predicate [x <= b] = 1
GC_f(x', b') revealed predicate [x <= b] = 1
GC_f(x', b') revealed predicate [x <= b] = 0
Encrypted loop iterations executed: 5
```

意義：

loop 從 encrypted `a'` 開始：

```text
x' = Enc(3)
b' = Enc(7)
one' = Enc(1)
```

每一輪只揭露：

```text
[x <= b]
```

在 `MOCK_OPENFHE` 模式下，每一輪的 `GC_f(x', b')` 都會重新做：

```text
current x', b' bits -> input labels -> garbled tables -> output decode
```

也就是 loop 的停止條件已經走 EMP half-gates GC artifact；state update 則使用 client 提供的 encrypted constant：

```text
x' <- FHE.Add(x', one')
```

實際 predicate sequence 是：

```text
3 <= 7 -> 1
4 <= 7 -> 1
5 <= 7 -> 1
6 <= 7 -> 1
7 <= 7 -> 1
8 <= 7 -> 0
```

因此 loop body 執行 5 次。第 6 次 predicate 變成 0，loop 停止。

### 7. 結束訊息

輸出：

```text
Demo completed without decrypting a, b, or x.
```

意義：

demo 沒有呼叫：

```text
DecryptInteger(a')
DecryptInteger(b')
DecryptInteger(x')
```

也沒有印出目前 encrypted state 對應的 plaintext。

## 目前 demo 的安全語意

目前展示的安全語意是：

Evaluator 可以離線反覆查詢：

```text
GC_f(x', b') -> [x <= b]
```

Evaluator 可以知道 predicate sequence 和 iteration count。

Evaluator 不直接取得：

```text
x
b
hsk
```

目前 `hsk` 是固定寫進 GC 的 toy key bits：

```text
hsk = [1, 0, 1, 1]
```

它是 GC 內部 constant selected labels，不是公開的 0/1 label pairs。

## 目前仍是 mock 的部分

目前仍是 mock 或 demo 化的部分：

- `MOCK_OPENFHE` 的 ciphertext 只有 `.bit`，所以 demo 能直接把 encrypted state 的 bit 轉成 GC input labels；真實 OpenFHE ciphertext 還需要 serialization。
- LWE decryption arithmetic 目前固定 `hsk=[1,0,1,1]`、`a=[3,5,6,1]`、`q=16`，不是從真實 OpenFHE key/ciphertext 動態生成。
- Decode 目前是 noiseless `phase[2]`，還沒有完整 OpenFHE rounding/noise handling。
- EMP half-gates backend 已經是真實 GC library path；但目前採用 relaxed/offline demo label 發放模型，沒有做 OT、single-use enforcement 或 leakage 評估。
- in-repo Minimal GC backend 只保留為無 EMP 環境的 fallback，不是主要展示路徑。
- Client setup 和 evaluator loop 還在同一個 executable，`GC_f` artifact 尚未寫檔或跨程序載入。
- `demo_keys/future_demo_fhe_key_material.json` 是目前固定 demo key material 的描述，不是 production OpenFHE key serialization。

所以目前 demo 證明的是 offline GC control-flow 的資料流，不是完整 OpenFHE production integration。

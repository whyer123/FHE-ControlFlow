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
mask = [1, 1, 1, 1]
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
GC artifact name: g(c_x,c_b)=Dec(Eval([x<=b],c_x,c_b))
GC artifact gate count: 31
Public input label pairs: 8
Hardcoded secret/constant labels: 9
Circuit_g constant wires: 9
```

意義：

`GC artifact name` 表示目前被包進 GC 的函數：

```text
g(c_x,c_b)=Dec(Eval([x<=b],c_x,c_b))
```

`GC artifact gate count: 31` 表示 `Circuit_g` 目前有 31 個 Boolean gates。

`Public input label pairs: 8` 來自 4-bit `x'` 和 4-bit `b'`：

```text
x_0, b_0, x_1, b_1, x_2, b_2, x_3, b_3
```

每個 public input wire 有 0/1 兩個 labels。依照目前研究假設，evaluator 可以持有這些 labels，因此可以離線查詢不同 ciphertext 對應的 predicate。

`Hardcoded secret/constant labels: 9` 包含：

```text
mock_eval_pad_bit
mock_ct_mask_0..3
hardcoded_mock_hsk_0..3
```

這些不是 public 0/1 label pairs，而是 garbler 寫進 GC artifact 的 selected labels。尤其 `hardcoded_mock_hsk_0..3` 代表固定 hsk：

```text
hsk = [1, 0, 1, 1]
```

evaluator 可以使用這些 labels evaluate GC，但不會直接知道它們對應的 bit 值。

`Circuit_g constant wires: 9` 表示 Boolean circuit 內有 9 條 constant wires。

### 4. Circuit_g gate list

輸出會列出：

```text
g0: w8(not_b_0) <- NOT(w1(b_0))
...
g30: w47(predicate_bit) <- OUTPUT(w46(mock_dec_out))
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

後半段是 mock decryption：

```text
mock_predicate_ct_body = predicate_msg XOR <mask,hsk>
mock_dec_pad = <mask,hsk>
mock_dec_out = mock_predicate_ct_body XOR mock_dec_pad
predicate_bit = mock_dec_out
```

因此 circuit 形狀已經是：

```text
Eval(f) -> Dec -> output predicate
```

### 5. Minimal GC evaluation

輸出：

```text
Minimal GC evaluated [a <= b] = 1
```

意義：

這一步在 `MOCK_OPENFHE` 模式下，把 `a'`、`b'` 的 mock ciphertext bits 編成 input labels，然後用 garbled tables evaluate `Circuit_g`。

它驗證：

```text
a = 3
b = 7
3 <= 7
```

所以輸出 predicate `1`。

這一步的重點是確認 GC artifact 的 label flow 可以跑通：

```text
input bits -> input labels -> garbled tables -> output label -> predicate bit
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
```

每一輪只揭露：

```text
[x <= b]
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

目前 mock decryption 是 toy LWE-like：

```text
ct_body = msg XOR <mask,hsk>
Dec(hsk, ct) = ct_body XOR <mask,hsk>
```

這不是 OpenFHE 真實 LWE decryption。下一步若要更接近真實系統，需要把 OpenFHE LWE ciphertext 的 decryption arithmetic 展開成 Boolean circuit，並定義 ciphertext serialization。

# GC Controlled Reveal Prototype

這份文件說明目前 prototype 如何把

```text
g(c_x, c_b) = FHE.Dec(hsk, FHE.Eval([x <= b], c_x, c_b))
```

整理成可往 garbled circuit 推進的形式，以及 demo 展示了什麼。

## 目標

Client 有 FHE secret key `hsk`，Evaluator 有加密狀態 `x'` 和加密邊界 `b'`。Evaluator 想要在不跟 Client 互動的情況下反覆判斷：

```text
[x <= b]
```

但 Evaluator 不能拿到 `x`、`b`，也不能拿到 `hsk`。

因此我們要做的不是：

```text
Dec(hsk, x')
```

而是固定成：

```text
Dec(hsk, Eval([x <= b], x', b'))
```

也就是只開放 predicate 的解密結果。

## 目前的實作邊界

目前 repo 已經做到三件事：

1. 用 FHE bit-level gates 計算 encrypted predicate。
2. 只解密 predicate bit。
3. 把 predicate circuit 描述成 stable wire/gate 格式，準備交給 GC backend。

目前還沒有把真實 OpenFHE 的 `Dec(hsk, ciphertext)` 完整展開成 Boolean circuit。也就是說，現在的 `ControlledRevealCircuit` 是 controlled reveal prototype：它把 `Eval(f)` 和「只解 predicate」放在同一個介面裡，先固定安全邊界與資料流。

## Step 1: FHE.Eval(f, c_x, c_b)

predicate 是：

```text
f(x, b) = [x <= b]
```

程式位置：

```text
src/gc/controlled_reveal_circuit.cpp
```

核心函式是：

```text
ControlledRevealCircuit::EvalLessOrEqualPredicate(x, bound)
```

它的輸入是 bit-level FHE ciphertext vector：

```text
x' = [x_0', x_1', ...]
b' = [b_0', b_1', ...]
```

它先用 ripple comparator 算：

```text
greater = [x > b]'
```

接著產生 mock predicate message：

```text
predicate_msg = NOT(greater) = [x <= b]
```

整個過程只使用 bit-level gates：

```text
AND, XOR, NOT
```

## Step 2: 只解密 predicate

程式位置：

```text
ControlledRevealCircuit::RevealPredicateOnly(predicate_ct)
```

目前 mock demo 使用固定的一組 toy secret key：

```text
hsk = [1, 0, 1, 1]
```

也使用固定的 mock ciphertext mask：

```text
mask = [1, 1, 1, 1]
```

mock ciphertext body 先被表示成：

```text
mock_eval_pad = <mask, hsk> mod 2
mock_predicate_ct_body = predicate_msg XOR mock_eval_pad
```

decryption 子電路會重新計算：

```text
mock_dec_term_i = mask_i AND hsk_i
mock_dec_pad = XOR_i(mock_dec_term_i)
mock_dec_out = mock_predicate_ct_body XOR mock_dec_pad
```

因為 `mock_dec_pad = mock_eval_pad`，所以 `mock_dec_out` 會回到原本的 predicate bit。

這仍然不是 OpenFHE 真實 LWE decryption。它的目的，是先讓 `Circuit_g` 的形狀明確包含：

```text
Eval(f) -> Dec
```

而且 `hsk` bits 是 hardcoded secret constant wires，會被 garble 進 GC artifact。

最後才輸出一個 bit：

```text
[x <= b]
```

它沒有呼叫：

```text
FHE.Dec(hsk, x')
FHE.Dec(hsk, b')
```

所以 demo 不會印出 `x`、`a`、`b`。

## Step 3: 把 g 包成同一個介面

程式位置：

```text
ControlledRevealCircuit::Evaluate(x, bound)
```

它做的就是：

```text
predicate_ct = EvalLessOrEqualPredicate(x, bound)
return RevealPredicateOnly(predicate_ct)
```

也就是：

```text
g(c_x, c_b) = Dec(hsk, Eval([x <= b], c_x, c_b))
```

上層 demo 不直接呼叫 decrypt。它只透過：

```text
EncryptedPredicateEvaluator::Evaluate(x', b')
```

這讓後面可以把目前的 controlled reveal prototype 換成真正的 garbled backend。

## Step 4: 把 Circuit_g 變成描述

程式位置：

```text
src/gc/boolean_circuit.h
src/gc/boolean_circuit.cpp
```

目前 `Circuit_g` 用 `BooleanCircuit` 表示：

```text
name
input_bit_length
input_wires
output_wires
constant_wires
wires
gates
```

每條 wire 都有穩定 id：

```text
w0, w1, w2, ...
```

每個 gate 都有穩定 id：

```text
g0, g1, g2, ...
```

demo 會印出類似：

```text
g0: w8(not_b_0) <- NOT(w1(b_0))
g1: w9(gt_0) <- AND(w8(not_b_0), w0(x_0))
...
g20: w28(predicate_msg) <- NOT(w27(gt_3))
g21: w30(mock_predicate_ct_body) <- XOR(w28(predicate_msg), w29(mock_eval_pad_bit))
...
g29: w41(mock_dec_out) <- XOR(w30(mock_predicate_ct_body), w40(mock_dec_pad_3))
g30: w42(predicate_bit) <- OUTPUT(w41(mock_dec_out))
```

這個格式的目的，是讓 GC backend 不需要解析字串公式，而是直接遍歷 gate list：

```text
for gate in circuit.gates:
    evaluate gate.inputs -> gate.output
```

## Step 5: Minimal GC backend

程式位置：

```text
src/gc/minimal_garbled_circuit.h
src/gc/minimal_garbled_circuit.cpp
```

這個 backend 是研究用的最小 GC artifact，不是最終密碼學安全版本。

它做三件事：

1. 對每條 wire 建立兩個 labels。
2. 對每個 gate 產生 garbled table。
3. 在 mock mode 下，把 input bits 編成 labels，沿著 garbled tables evaluate，最後 decode output label。

目前依照我們的新假設：

```text
Evaluator 可以持有所有 input labels。
```

因此這裡不做 OT，也不限制 single-use。公開給 Evaluator 的是 `x'`、`b'` 的 input labels、garbled tables、hardcoded hsk/constant 的 selected label，以及 output decoding table。安全目標不是防止 Evaluator 查詢所有 predicate，而是讓 Evaluator 只能查：

```text
g(c_x, c_b)
```

不能拿到：

```text
hsk
Dec(hsk, x')
Dec(hsk, b')
```

## Demo 展示流程

demo 在：

```text
examples/demo_loop.cpp
```

流程是：

1. 建立 FHE context 和 keys。
2. 建立 bit-level FHE gates。
3. 建立 `ControlledRevealCircuit`。
4. 加密起點和終點：

```text
a' = Enc(a)
b' = Enc(b)
```

目前 mock demo 使用：

```text
a = 3
b = 7
bit_length = 4
```

5. 產生 `Circuit_g` 描述。
6. 產生 minimal garbled artifact。
7. 在 mock mode 下把 `a'`、`b'` 的 bit 值編成 input labels，驗證 minimal GC 對 `[a <= b]` 的輸出。
8. 執行 evaluator loop：

```text
x' = a'
while true:
    cond = GC_f(x', b')
    if cond == 0:
        stop
    x' = x' + 1'
```

目前 demo 會揭露：

```text
[x <= b]
iteration count
```

目前 demo 不揭露：

```text
a
b
x
hsk
```

## 目前輸出的意義

當 `a=3, b=7` 時，loop 會檢查：

```text
3 <= 7 -> 1
4 <= 7 -> 1
5 <= 7 -> 1
6 <= 7 -> 1
7 <= 7 -> 1
8 <= 7 -> 0
```

所以會執行 5 輪，然後停止。

這表示 Evaluator 學到 runtime 和 predicate sequence，但沒有看到 `x` 的明文值。這符合目前接受的 leakage model。

## 下一步

下一個真正重要的研究步驟，是把：

```text
FHE.Dec(hsk, predicate_ct)
```

也展開成 Boolean circuit，讓 `Circuit_g` 不只描述 comparator，也包含 secret-key-dependent decryption logic。到那一步，Client 才能真正把 `hsk` 包進 garbled artifact，而不是在 C++ prototype 裡直接呼叫 decrypt。

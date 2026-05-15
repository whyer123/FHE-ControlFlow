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

目前 repo 已經做到四件事：

1. 用 FHE bit-level gates 計算 encrypted predicate。
2. 只解密 predicate bit。
3. 把 predicate circuit 描述成 stable wire/gate 格式，準備交給 GC backend。
4. 把 OpenFHE LWE decryption 的核心 phase arithmetic 展開成 Boolean circuit。

目前的 decryption circuit 還不是完整 production OpenFHE decrypt。它已經展開：

```text
phase = b - <a,hsk> mod q
rounded = phase + q/(2p)
result = floor(p * rounded / q)
```

以及 demo 用的 rounding/decode shape；但還沒有把真實 OpenFHE ciphertext serialization 接進 circuit input。

也就是說，現在的 `ControlledRevealCircuit` 已經固定 controlled reveal 的安全邊界與資料流，並開始把 `Dec(hsk, predicate_ct)` 的 arithmetic 放進 `Circuit_g`。

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

接著產生 predicate message：

```text
predicate_msg = NOT(greater) = [x <= b]
```

整個過程只使用 bit-level gates：

```text
AND, XOR, NOT
```

## Step 2: OpenFHE LWE decryption arithmetic

runtime prototype 的程式位置：

```text
ControlledRevealCircuit::RevealPredicateOnly(predicate_ct)
```

Boolean circuit 版本目前使用固定的一組 OpenFHE LWE-like secret key：

```text
hsk = [1, 0, 1, 1]
```

也使用固定的 LWE ciphertext mask coefficients：

```text
a = [3, 5, 6, 1]
```

目前 demo modulus 固定為：

```text
q = 16
scale = q / 4 = 4
```

predicate message 先被 encode 到 LWE phase：

```text
encoded_msg = predicate_msg * scale
```

接著產生 demo ciphertext body：

```text
b = encoded_msg + <a, hsk> mod q
```

decryption 子電路會重新計算：

```text
pad = <a, hsk> mod q
phase = b - pad mod q
rounded = phase + q/(2p)
predicate_bit = floor(p * rounded / q)
```

因為 demo 使用 `q=16`、`p=4`，所以 `floor(p * rounded / q)` 的 predicate bit 對應 `rounded[2]`。

這仍然不是完整 OpenFHE production decryption，因為 OpenFHE 真實 decrypt 還有更完整的 modulus/rounding/noise 處理。它的目的，是先把 LWE decryption arithmetic 的核心形狀展成 Boolean circuit：

```text
Eval(f) -> LWE body b -> b - <a,hsk> mod q -> decode bit
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

目前 demo 在 `MOCK_OPENFHE` 模式下已經把這個介面接到 `GarbledPredicateEvaluator`。也就是說，loop 每一輪的 predicate 判斷會走：

```text
x', b' mock ciphertext bits
-> GC input labels
-> garbled tables
-> output label
-> [x <= b]
```

完整 OpenFHE 模式目前仍保留 direct controlled reveal fallback，因為真實 OpenFHE ciphertext serialization 還沒有接到 GC input。

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
...
openfhe_lwe_ct_body_*     // b = encoded_msg + <a,hsk> mod q
openfhe_lwe_phase_*       // phase = b - <a,hsk> mod q
openfhe_lwe_rounded_phase_* // phase + q/(2p)
predicate_bit             // floor(p * rounded / q)
```

這個格式的目的，是讓 GC backend 不需要解析字串公式，而是直接遍歷 gate list：

```text
for gate in circuit.gates:
    evaluate gate.inputs -> gate.output
```

## Step 5: EMP half-gates GC backend

程式位置：

```text
src/gc/emp_garbled_circuit.h
src/gc/emp_garbled_circuit.cpp
src/gc/minimal_garbled_circuit.h
src/gc/minimal_garbled_circuit.cpp
```

目前 Docker `gc_mock` target 使用 EMP-toolkit 的 half-gates backend。`minimal_garbled_circuit.*` 保留成不裝 EMP 時的本機 fallback，不是主要展示路徑。

EMP backend 做三件事：

1. 對 public input wires 建立 0/1 label pair。
2. 對 hardcoded `hsk` 和其他 constant wires 只放入 selected label。
3. 用 `emp::HalfGateGen` 產生 AND gate 的 half-gates transcript，XOR/NOT 走 free-XOR/free-NOT label flow。
4. 在 mock mode 下，`GarbledPredicateEvaluator` 把每一輪的 input bits 選成 labels，再用 `emp::HalfGateEva` 沿著同一份 transcript evaluate，最後 decode output label。

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

注意：為了符合「evaluator 可以離線重複查 predicate」這個 demo 假設，EMP artifact 目前也保存 free-XOR 需要的 `delta` 和 output decode material。這不是標準一次性 GC 的 label 發放模型；它是我們目前用來隱藏 `hsk`、但允許重複查 `g(c_x,c_b)` 的 controlled-reveal 原型模型。

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
one' = Enc(1)
```

目前 mock demo 使用：

```text
a = 3
b = 7
bit_length = 4
```

5. 產生 `Circuit_g` 描述，並輸出到：

```text
artifacts/circuit_g_demo.txt
```

6. 在 Docker `gc_mock` 中產生 EMP half-gates garbled artifact；未設定 `USE_EMP_GC` 時才使用 minimal fallback。
7. 建立 `GarbledPredicateEvaluator`，把 `Circuit_g` artifact 包成 `GC_f(x', b') -> bool`。
8. 在 mock mode 下把 `a'`、`b'` 的 bit 值編成 input labels，驗證 EMP GC 對 `[a <= b]` 的輸出。
9. 執行 evaluator loop，而且每一輪 predicate 都重新 evaluate garbled artifact：

```text
x' = a'
while true:
    cond = GC_f(x', b')
    if cond == 0:
        stop
    x' = FHE.Add(x', one')
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

## Fixed-GC Evaluator Runtime Demo

目前新增一條更接近最後目標的 fixed runtime 路徑：

```text
setup_fixed_gc_material:
  建立 fixed-bound Circuit_g
  將 b 固定為 selected-label constant
  garble 成 fixed GC_f
  寫出 circuit shape 與 GC artifact

fixed_runtime_demo:
  載入 a'
  載入 one'
  載入 fixed GC_f
  不載入 hpk
  不載入 hsk
  不接受自由 b' input
```

執行方式：

```bash
./build_mock.sh --fixed-setup
./build_mock.sh --fixed-runtime
```

fixed-bound circuit dump 在：

```text
artifacts/fixed_bound_circuit_g_demo.txt
```

這份 dump 的 public input 只剩：

```text
x_0, x_1, x_2, x_3
```

不再有：

```text
b_0, b_1, b_2, b_3
```

`fixed_b_i`、toy `hsk_i` 和 decryption constants 都是 fixed constant wires。dump 會用：

```text
<selected-label>
```

取代實際 constant bit，因為 evaluator runtime 需要的是 selected labels，不應該拿到 fixed `b` 或 `hsk` 的明文 bit。

runtime 實際讀的是：

```text
artifacts/fixed_bound_circuit_shape.bin
artifacts/fixed_bound_gc_artifact.bin
artifacts/a_prime_mock_bits.txt
artifacts/one_prime_mock_bits.txt
```

其中 `fixed_bound_circuit_shape.bin` 只保存 gate/wire shape，不保存 constant bit 值；`fixed_bound_gc_artifact.bin` 保存 selected labels 和 garbled tables。

因此第一版 fixed runtime 的查詢介面是：

```text
GC_f(x')
```

不是：

```text
GC_f(x', b')
```

這避免 evaluator 把同一個 `x'` 拿去查不同 threshold，例如 `x <= 10'`、`x <= 11'`。

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

## 目前仍是 mock 的部分

目前仍是 mock 或 demo 化的部分：

1. `MOCK_OPENFHE` 的 ciphertext 只有一個 `.bit`，所以 GC input label encoding 目前仍是從 mock bit 直接取得。
2. fixed runtime 已經有 artifact serialization，但 mock runtime 使用的是 mock `a'`/`one'` bit material，不是真實 OpenFHE ciphertext fields。
3. LWE decryption circuit 使用固定 demo key、固定 mask、固定 `q=16`，不是從真實 OpenFHE ciphertext 解析出來。
4. decode 已加入 demo 版 `phase + q/(2p)` rounding shape，但仍未接真實 OpenFHE ciphertext fields 和 production 參數。
5. 已新增 OpenFHE runtime material check，能證明 evaluator 可只載入 context/evaluation keys、`a'`、integer `one'` 做一次 encrypted add；但它尚未把真實 OpenFHE ciphertext bits 接到 GC input。
6. EMP half-gates backend 已經是真實 GC library path，但目前使用的是 relaxed/offline demo label 發放模型：evaluator 可以持有 public input 的所有 labels 和 output decode material。
7. in-repo minimal GC 仍保留為無 EMP 環境的 fallback，不是主要 demo path。

## 下一步

接下來要對齊 production OpenFHE 與離線流程：

```text
real LWE ciphertext fields -> circuit inputs
OpenFHE rounding/noise decode -> Boolean circuit
client-side garble -> serialized GC_f
evaluator-side offline loop -> GC_f(x', b')
```

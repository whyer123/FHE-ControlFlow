# Fixed Demo Material

這份檔案定義目前 demo 要固定下來的 material。目標是：

```text
Client setup 已經完成：
  a' = Enc(a)
  b' = Enc(b)
  one' = Enc(1)
  GC_f = Garble(Circuit_g with fixed hsk)

Evaluator runtime 只拿：
  a'
  b'
  one'
  GC_f

Evaluator 不拿：
  hpk
  hsk
```

## Fixed hpk / hsk status

目前 prototype 使用的是 OpenFHE BinFHE/LWE bit-ciphertext 路線。這條路線在目前 code 裡是用 LWE private key encrypt bit，不是 public-key FHE encryption API，所以現在沒有可展示、可序列化的 `hpk`。

因此目前固定 key material 是：

```text
hpk = none in current BinFHE prototype
hsk = [1, 0, 1, 1]
```

這不是說最終設計不能有 `hpk`。如果 production demo 必須真的展示 `hpk/hsk` pair，就要把 encryption path 切到 OpenFHE public-key scheme，或確認並接上 BinFHE 的 public-key encryption support。現在這一步先不做，因為 evaluator 目標本來就是不需要 `hpk`。

## Fixed LWE-like decryption parameters

目前要編進 GC 的 toy LWE-like decryption arithmetic 使用：

```text
q = 16
modulus_bits = 4
hsk = [1, 0, 1, 1]
mask a = [3, 5, 6, 1]
decode = phase[2]
```

在 Boolean circuit 裡，`hsk` 被 hardcode 成 constant wires：

```text
w29 hardcoded_openfhe_lwe_hsk_0 = 1
w34 hardcoded_openfhe_lwe_hsk_1 = 0
w39 hardcoded_openfhe_lwe_hsk_2 = 1
w44 hardcoded_openfhe_lwe_hsk_3 = 1
```

mask `a` 也被 hardcode 成 constant wires，例如：

```text
openfhe_lwe_a_0 = 3 = 0011
openfhe_lwe_a_1 = 5 = 0101
openfhe_lwe_a_2 = 6 = 0110
openfhe_lwe_a_3 = 1 = 0001
```

## Circuit to garble

目前要編成 GC 的 circuit 已 dump 在：

```text
artifacts/circuit_g_demo.txt
```

目前大小：

```text
input_bit_length = 4
wire_count = 382
gate_count = 338
public_inputs = x_0,b_0,x_1,b_1,x_2,b_2,x_3,b_3
output = predicate_bit
```

它表示：

```text
Circuit_g(c_x, c_b)
  = FHE.Dec(hsk, FHE.Eval([x <= b], c_x, c_b))
```

目前的 circuit 分三段：

```text
1. comparator:
   predicate_msg = [x <= b]

2. toy LWE-like encryption/decryption arithmetic:
   ct_body = encoded(predicate_msg) + <a,hsk> mod q
   recomputed_pad = <a,hsk> mod q
   phase = ct_body - recomputed_pad mod q

3. decode:
   predicate_bit = phase[2]
```

## Runtime loop semantics

固定 demo 的 evaluator loop 是：

```text
x' = a'

while true:
    cond = GC_f(x', b')
    if cond == 0:
        stop
    x' = FHE.Add(x', one')
```

`one' = Enc(1)` 是 client setup 時給 evaluator 的 encrypted constant。Evaluator 不需要 `hpk` 來自己加密 `1`。

## Current caveat

目前 `hsk` 和 LWE-like decryption arithmetic 已固定且可看；`GC_f` 也已經可以用 EMP half-gates backend evaluate。

還沒有完成的是：

```text
serialized fixed GC artifact
real OpenFHE ciphertext serialization
real OpenFHE public-key hpk/hsk material
```

所以目前可展示的是固定 `hsk + fixed Circuit_g + EMP GC runtime`，不是完整 production OpenFHE key serialization。

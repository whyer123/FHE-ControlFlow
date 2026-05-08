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

目前已產生一組真正 OpenFHE BinFHE/LWE public-key demo key pair：

```text
demo_keys/openfhe_binfhe_demo_keypair/hpk_lwe_public_key.json
demo_keys/openfhe_binfhe_demo_keypair/hpk_lwe_public_key.bin
demo_keys/openfhe_binfhe_demo_keypair/hsk_lwe_secret_key.json
demo_keys/openfhe_binfhe_demo_keypair/hsk_lwe_secret_key.bin
```

產生方式是：

```text
BinFHEContext.GenerateBinFHEContext(TOY)
hsk <- KeyGen()
BTKeyGen(hsk, PUB_ENCRYPT)
hpk <- GetPublicKey()
```

並已做過自測：

```text
Dec_hsk(Enc_hpk(0)) = 0
Dec_hsk(Enc_hpk(1)) = 1
EvalBinGate(AND, Enc_hpk(1), Enc_hpk(1)) = 1
```

Evaluator 若要在 BinFHE ciphertext 上做 gate evaluation，除了 ciphertext 之外也需要 public evaluation material：

```text
demo_keys/openfhe_binfhe_demo_keypair/binfhe_context_params.bin
demo_keys/openfhe_binfhe_demo_keypair/eval_refresh_key.bin
demo_keys/openfhe_binfhe_demo_keypair/eval_switch_key.bin
```

不過目前固定 GC circuit 還沒有改成使用這組真 OpenFHE `hsk`。目前 `artifacts/circuit_g_demo.txt` 裡的 decryption circuit 仍是 toy LWE-like hsk：

```text
hsk = [1, 0, 1, 1]
```

也就是說，現在有兩層 material：

```text
1. real OpenFHE BinFHE hpk/hsk files:
   已生成，可展示，可做 public-key encrypt/decrypt 自測。

2. current GC demo hsk:
   仍是 4-bit toy hsk，用來讓 Circuit_g 小到能清楚展示。
```

下一步若要完全對齊，就要把 real OpenFHE `hsk` 和 ciphertext layout 展開進 `Circuit_g`，再重新 garble。

## Prepared encrypted constants

已經用上面那組 `hpk` 先準備好：

```text
one' = Enc_hpk(1)
```

檔案在：

```text
demo_keys/openfhe_binfhe_demo_keypair/one_prime_lwe_ciphertext.json
demo_keys/openfhe_binfhe_demo_keypair/one_prime_lwe_ciphertext.bin
demo_keys/openfhe_binfhe_demo_keypair/encrypted_constants_manifest.md
```

已用 `hsk` 驗證：

```text
Dec_hsk(one') = 1
```

因此 evaluator loop 可以直接拿 `one'` 做：

```text
x' <- FHE.Add(x', one')
```

不需要拿 `hpk` 自己加密常數。

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
real OpenFHE hsk expanded into Circuit_g
```

所以目前可展示的是：

```text
real OpenFHE BinFHE hpk/hsk files
prepared one_prime = Enc_hpk(1)
fixed toy-hsk Circuit_g
EMP GC runtime
```

還不是「real OpenFHE hsk 已經編進 Circuit_g」的版本。

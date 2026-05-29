# Demo Loop Report

這份 report 專門說明目前 `examples/demo_loop.cpp` 的 demo 在做什麼，以及執行輸出的每一段代表什麼。

## OpenFHE reference logic

現在第一步改成一個準備 lowering 成 Boolean circuit 的單檔邏輯：

```text
src/gc/openfhe_controlled_reveal_reference.cpp
```

這個檔案沒有：

```text
main
filesystem path
serialization
print/stdout
demo key directory
OpenFHE include/runtime types
```

它把要轉成 Boolean circuit 的核心邏輯集中在同一檔：

```text
Dec_hsk(OpenFHE.Eval([x <= b], x', b'))
```

這一檔只保留 `GC_f` 需要的 controlled reveal，不放 loop update helper `x' <- x' + one'`。

私鑰已經固定在同一檔的 `FIXED_HSK`。之後轉 GC 時，`FIXED_HSK` 會變成 fixed secret constants / selected labels，`x'` 和 `b'` 會變成 runtime input wires。

目前測試是直接 include 這個 `.cpp` 到 C++ test target。

這個 source 使用 STD128-shaped LWE dimension/modulus constants，但測試 target 不 link OpenFHE、不使用 `demo_keys/openfhe_binfhe_demo_keypair`：

```bash
cmake -S . -B build-local -DCMAKE_BUILD_TYPE=Release
cmake --build build-local --target openfhe_controlled_reveal_reference_test --parallel 2
./build-local/openfhe_controlled_reveal_reference_test
```

這個測試程式只用 exit code 驗證，不依賴 reference logic print 資訊。

這個檔案的用途是先確認正式邏輯；下一步才把同一段邏輯 lowering 成 Boolean circuit。

## Fixed Runtime Demo

目前另有一個更接近最後目標的 evaluator-runtime-only demo：

```bash
./build_mock.sh --fixed-setup
./build_mock.sh --fixed-runtime
```

`--fixed-setup` 是 setup/client 端預先工作，會建立：

```text
artifacts/fixed_bound_circuit_g_demo.txt
artifacts/fixed_bound_circuit_shape.bin
artifacts/fixed_bound_gc_artifact.bin
artifacts/a_prime_mock_bits.txt
artifacts/one_prime_mock_bits.txt
```

`--fixed-runtime` 只展示 evaluator runtime：

```text
x' = a'
while true:
    cond = GC_f(x')
    if cond == 0:
        stop
    x' = FHE.Add(x', one')
```

runtime 輸出：

```text
Evaluator runtime input policy: fixed GC_f(x')
Evaluator runtime loaded only a', one', evaluation material, and fixed GC_f
Evaluator runtime did not load hpk or hsk
```

意思是這條 demo path 不載入 `hpk`、不載入 `hsk`，也不接受自由的 `b'` input。`b` 已經在 setup 階段固定進 GC artifact 的 selected labels。

predicate sequence 仍是：

```text
3 <= 7 -> 1
4 <= 7 -> 1
5 <= 7 -> 1
6 <= 7 -> 1
7 <= 7 -> 1
8 <= 7 -> 0
```

所以輸出：

```text
Encrypted loop iterations executed: 5
```

這條 fixed runtime demo 目前仍使用 `MOCK_OPENFHE` bit material 連到 GC input；真實 OpenFHE ciphertext fields 尚未接進 GC input。

## OpenFHE LWE Integer Runtime Demo

目前 v2 路線已經有一條接近目標的 runtime demo。它不是舊的 fixed-bound mock，而是使用 OpenFHE 真的產生的單一 integer LWE ciphertext fields：

```text
a' = Enc_hpk(3)
b' = Enc_hpk(7)
one' = Enc_hpk(1)
```

setup 端先執行：

```bash
./build-local/export_openfhe_lwe_int_material \
    demo_keys/openfhe_binfhe_demo_keypair \
    /tmp/full_v2_lwe_integer_material.txt

./build-local/setup_openfhe_lwe_int_gc_material \
    /tmp/full_v2_lwe_integer_material.txt \
    /tmp/openfhe_lwe_int_runtime
```

第一步會用現有 OpenFHE `hpk/hsk` 產生 `a'`、`b'`、`one'`，並匯出 LWE fields 與 `hsk.s_mod_q`。第二步是 setup/client side：它用 `hsk.s_mod_q` 建立 Boolean circuit 和 GC artifact，然後輸出 runtime 需要的 sanitized material。

runtime 端只執行：

```bash
./build-local/openfhe_lwe_int_runtime_demo /tmp/openfhe_lwe_int_runtime
```

runtime 載入：

```text
openfhe_lwe_int_runtime_material.txt
openfhe_lwe_int_circuit_shape.bin
openfhe_lwe_int_gc_artifact.bin
```

runtime 不載入：

```text
hpk_lwe_public_key.*
hsk_lwe_secret_key.*
full_v2_lwe_integer_material.txt
```

這條 demo 的 GC 語意是：

```text
GC_f(x', b') = GC{ [Dec_hsk(x') <= Dec_hsk(b')] }
```

loop 輸出應該是：

```text
Predicate sequence: 1,1,1,1,1,0
Encrypted loop iterations executed: 5
```

也就是：

```text
3 <= 7 -> 1
4 <= 7 -> 1
5 <= 7 -> 1
6 <= 7 -> 1
7 <= 7 -> 1
8 <= 7 -> 0
```

這裡的 `x' <- x' + one'` 是 LWE ciphertext fields 的 component-wise addition mod `q`。它符合目前 integer LWE demo 的加法語意，不需要 evaluator 持有 `hpk`。如果之後回到 bit-level OpenFHE `EvalBinGate` 更新，才需要另外處理 evaluation keys / bootstrapping。

## EvalBinGate Circuit Dump

可以用下面指令直接看目前 `OpenFHE.EvalBinGate` 被展成 Boolean circuit 的樣子：

```bash
./build_mock.sh --evalbingate-circuit
```

輸出會包含：

```text
Dumped OpenFHE EvalBinGate Boolean circuits to artifacts
bootstrap core status: placeholder
and: 477 gates, 558 wires, dump=artifacts/openfhe_evalbingate_and_demo.txt
or: 479 gates, 560 wires, dump=artifacts/openfhe_evalbingate_or_demo.txt
xor: 617 gates, 699 wires, dump=artifacts/openfhe_evalbingate_xor_demo.txt
xnor: 618 gates, 700 wires, dump=artifacts/openfhe_evalbingate_xnor_demo.txt
```

這些 dump 的意義：

```text
lhs_a_i_bit_j / lhs_b_bit_j: 第一個 LWE ciphertext 的 fields
rhs_a_i_bit_j / rhs_b_bit_j: 第二個 LWE ciphertext 的 fields
openfhe_evalbingate_prebootstrap_*: OpenFHE EvalBinGate 前段的 ciphertext add/double
openfhe_bootstrap_placeholder_*: 尚未正式展開的 bootstrap core placeholder
evalbingate_out_*: 輸出的 LWE ciphertext fields
```

目前這不是正式版 `EvalBinGate`。已完成的是 LWE-shaped circuit interface、pre-bootstrap arithmetic、固定 demo hsk selected-label constants、以及 output ciphertext shape；還沒完成的是真正 OpenFHE `BootstrapGateCore` 的 blind rotation/RGSW 展開。

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
GC artifact gate count: 366
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

`GC artifact gate count: 366` 表示 `Circuit_g` 目前在 4-bit demo 參數下有 366 個 Boolean gates。這個數字會隨 bit length、LWE dimension、modulus bit width、decode logic 變動。

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
g365: w414(predicate_bit) <- OUTPUT(w401(openfhe_lwe_rounded_phase_sum_2))
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

目前主要 mock GC runtime 裡，`hsk` 仍是早期固定 toy key bits：

```text
hsk = [1, 0, 1, 1]
```

它是 GC 內部 constant selected labels，不是公開的 0/1 label pairs。

新的 `src/gc/openfhe_controlled_reveal_reference.cpp` 則已經改成使用已生成 OpenFHE TOY keypair 裡的實際 64 個 ternary `hsk` 係數，並用實際 OpenFHE `Enc(1)` / `Enc(0)` ciphertext 驗證 `Dec_hsk` 算術。這個 reference source 是接下來要 lowering 成 GC 的版本，還沒接到主要 runtime。

另外已新增 `export_openfhe_eval_key_material`，可以把固定 demo 的 `eval_refresh_key.bin` / `eval_switch_key.bin` 匯出成 plain C++ integer arrays。實測 material 約 70MB，refresh key 有 `524288` 個 words，switch key A 有 `4915200` 個 words，switch key B 有 `76800` 個 words。

另外已新增 `export_openfhe_lwe_int_material`，這是 v2 `GC{[Dec(x') <= Dec(b')]}` 路線的 OpenFHE material bridge。它會用現有 OpenFHE `hpk/hsk` 產生單一 integer ciphertext，而不是 4 個 bit ciphertext：

```text
a' = Enc_hpk(3; p=16)
b' = Enc_hpk(7; p=16)
one' = Enc_hpk(1; p=16)
```

工具會匯出每個 ciphertext 的 `a` vector、`b` body、`q`、`p`，以及切到 ciphertext modulus 後的 `hsk.s_mod_q`。目前實測輸出是：

```text
n = 64
q = 512
p = 16
```

並且已驗證：

```text
OpenFHE Decrypt(ct) == exported-field Dec_hsk(ct)
```

這代表 OpenFHE 真實 LWE ciphertext / secret key 已經能被抽成之後 Boolean circuit lowering 需要的資料格式。

`export_openfhe_lwe_int_material` 現在也支援指定 demo plaintext：

```text
export_openfhe_lwe_int_material <key-dir> <output-path> <a> <b> [one]
```

例如：

```text
export_openfhe_lwe_int_material demo_keys/openfhe_binfhe_demo_keypair /tmp/mat_8_7.txt 8 7 1
```

這會用同一組 OpenFHE `hpk/hsk` 重新產生 `a'=Enc(8)`、`b'=Enc(7)`、`one'=Enc(1)`。`test_openfhe_lwe_int_multiple_values.sh` 會用這個介面驗證多組 true/false comparison，而不是只驗固定 `3<=7`。

setup-side material 目前有明確格式版本：

```text
format = openfhe_lwe_int_setup_material_v1
```

runtime-side material 則是：

```text
format=openfhe_lwe_int_runtime_material_v1
```

parser 會拒絕沒有 format version 的 setup material，避免舊的 ad-hoc dump 被誤用成正式 v2 material。

目前也已新增 `OpenFHELWEIntDecryptCompareCircuit`，將上述 fields 接進 v2 Boolean circuit：

```text
GC{ [Dec_hsk(x') <= Dec_hsk(b')] }
```

其中 `hsk.s_mod_q` 不再當成 public constant wire，而是透過 `secret_constant_wires` 進 circuit。這代表 plain circuit evaluator 和之後 GC setup 可以知道 secret bit/value，但 redacted circuit dump 不會把 `hsk` 數值印出來。

現在 v2 測試不再手抄 exported fields，而是每次先執行 `export_openfhe_lwe_int_material` 產生臨時 material 檔，再由 `OpenFHELWEIntMaterial` parser 讀入 `a'`、`b'`、`one'` 和 `hsk.s_mod_q`。現階段已驗證：

```text
Dec(a') <= Dec(b') -> 1
Dec(b') <= Dec(a') -> 0
Dec(b') <= Dec(b') -> 1
```

這代表 v2 circuit 已經接上 OpenFHE 真實產生的 LWE ciphertext / secret key material，而不是固定手抄 fixture。

目前已新增 v2 GC 測試：

```text
bash tests/test_openfhe_lwe_int_decrypt_compare_gc.sh
```

它會先動態產生 OpenFHE material，再把 `OpenFHELWEIntDecryptCompareCircuit` garble 成 artifact，用 material 裡的 ciphertext bits evaluate predicate。預設本機使用 in-repo minimal GC backend；若在 Docker `gc_mock` 內執行：

```text
./build_mock.sh --v2-openfhe-gc-test <material>
```

且 `USE_EMP_GC=1`，則會使用 EMP half-gates backend。這代表 v2 circuit 已接到 GC artifact/evaluation API；但本機尚未驗證 EMP，因為 host 沒有 EMP toolkit，且目前 Docker daemon 未啟動。

目前也新增 v2 runtime demo：

```text
bash tests/test_openfhe_lwe_int_runtime_demo.sh
```

這個測試會檢查 runtime material 不含 `hsk`、不含 clear plaintext/manual decrypt fields，並確認 evaluator runtime 能只靠 `a'`、`b'`、`one'`、circuit shape、GC artifact 跑出 `1,1,1,1,1,0`。

測試中也會執行：

```text
audit_openfhe_lwe_int_runtime_boundary
```

這個 audit 會讀 full setup material 作為正向對照，確認 full material 確實含有 `hsk.s_raw`、`hsk.s_mod_q` 和 `manual_dec`；接著檢查 runtime directory 沒有這些 setup-only field、沒有 clear plaintext field、沒有 `hpk/hsk` key file reference。它也會讀 serialized circuit shape，確認 secret constants 只剩 wire ids，readback 後沒有 secret values。

另有一個 GC-only build 入口：

```text
./build_mock.sh --v2-openfhe-runtime-demo <material> <runtime-dir>
```

這個入口不負責產生 OpenFHE material；它假設 setup/client 已經產好 material 檔。它會在 GC-only 環境中執行：

```text
setup_openfhe_lwe_int_gc_material
audit_openfhe_lwe_int_runtime_boundary
openfhe_lwe_int_runtime_demo
```

因此在本機沒設 `USE_EMP_GC` 時會走 minimal GC fallback；在 Docker `gc_mock` 裡因 `USE_EMP_GC=1` 會走 EMP half-gates backend。這是目前要驗證 EMP v2 runtime 的主要入口。

另外也新增一個完整容器服務：

```text
docker-compose run --rm openfhe_emp_v2_runtime
```

這個 service 使用 `openfhe-emp-runtime` Docker target。容器內同時有 OpenFHE 與 EMP，所以它會在同一個環境中：

```text
1. 產生 OpenFHE LWE integer material。
2. 用 `hsk.s_mod_q` 建 v2 GC artifact。
3. 執行 runtime boundary audit。
4. 用 EMP half-gates backend 跑 `openfhe_lwe_int_runtime_demo`。
5. 檢查 `Predicate sequence: 1,1,1,1,1,0`。
```

## 目前仍是 mock 的部分

目前仍是 mock 或 demo 化的部分：

- 舊的 `MOCK_OPENFHE` / fixed-bound runtime path 仍存在，主要作為早期比較用；v2 OpenFHE integer LWE runtime 已改用真實 OpenFHE 匯出的 ciphertext fields。
- v2 runtime 目前使用 OpenFHE TOY/test key material，還不是 production security parameters。
- v2 runtime 的 GC artifact 已經把 `hsk.s_mod_q` 變成 selected labels，並新增 serialization boundary audit；但這不是 reusable GC 的密碼學安全證明。
- v2 runtime 的 `x' <- x' + one'` 是 integer LWE ciphertext component-wise addition；如果之後要回到 bit-level OpenFHE `EvalBinGate` loop update，仍要處理 evaluation keys / bootstrapping。
- Docker `openfhe_emp_v2_runtime` 的 EMP 實跑目前還沒完成，因為本機 Docker daemon 連不上；Dockerfile/compose 入口已準備好，等 Docker daemon 啟動即可跑。
- `openfhe_controlled_reveal_reference.cpp` 已有真實 OpenFHE TOY hsk、`q=512`、`phase + q/(2p)` rounding，並移除 semantic gate decode；但 `EvalAccCGGI`、`ExternalProductCGGI`、`SwitchCTtoqn` 還不是 OpenFHE 1.5.0 bit-accurate bootstrap/key-switch 展開。
- `export_openfhe_eval_key_material` 已能產生固定 eval-key integer arrays，但這些 flat words 還沒被 exact layout 接入 standalone controlled-reveal source。
- EMP half-gates backend 已經是真實 GC library path；但目前採用 relaxed/offline demo label 發放模型，沒有做 OT、single-use enforcement 或 leakage 評估。
- in-repo Minimal GC backend 只保留為無 EMP 環境的 fallback，不是主要展示路徑。
- v2 setup 和 runtime 已拆成兩個 executable，但還不是完整 client/server packaging。
- `demo_keys/future_demo_fhe_key_material.json` 是目前固定 demo key material 的描述，不是 production OpenFHE key serialization。

所以目前 demo 證明的是 offline GC control-flow 的資料流，不是完整 OpenFHE production integration。

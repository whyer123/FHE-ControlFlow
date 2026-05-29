# TODO

## 已完成

- 移除 ABE2 selector prototype，專案方向改成 FHE comparison algorithm + GC controlled reveal。
- 建立 `ControlledRevealCircuit`，把 `FHE.Eval([x <= b])` 和只解密 predicate bit 包成同一個介面。
- 新增 `EncryptedPredicateEvaluator`，讓 evaluator loop 只依賴 `GC_f(x', b') -> bool`。
- 新增 encrypted add，demo 可以跑 encrypted loop：`x' = a'`，每輪 `x' <- FHE.Add(x', one')`，其中 `one'=Enc(1)` 由 client 提供。
- 新增 `BooleanCircuit`，用 stable `WireId`、`GateId`、input wires、output wires 描述 `Circuit_g`。
- 新增 minimal GC backend，產生 wire labels、garbled tables，並用 label flow 做 mock evaluation。
- 新增 `GarbledPredicateEvaluator`，讓 mock demo loop 每一輪都直接 evaluate `Circuit_g` garbled artifact。
- 新增 OpenFHE LWE-like decryption circuit：固定 `hsk=[1,0,1,1]`、`a=[3,5,6,1]`、`q=16`、`p=4`，展開 `b - <a,hsk> mod q`、`phase + q/(2p)` rounding 和 bit decode。
- 新增 EMP-toolkit half-gates backend，Docker `gc_mock` 會安裝 EMP 並用 `USE_EMP_GC=1` 真實執行 EMP GC path。
- 新增 `artifacts/circuit_g_demo.txt`，不用只靠 runtime stdout 也能檢查 `FHE.Dec(hsk,FHE.Eval(f,x'))` 的 Boolean circuit gate list。
- 新增 `demo_keys/future_demo_fhe_key_material.json`，記錄目前 demo 使用的固定 hsk 與 `Enc(1)` 設計決策。
- 新增 `gc.md`，用中文說明 controlled reveal、circuit 描述、EMP GC 和 demo leakage model。
- 新增 fixed-bound GC path：`GC_f(x')`，不再把 `b'` 當 runtime free input。
- 新增 `setup_fixed_gc_material`，預先輸出 fixed-bound circuit shape、GC artifact、mock `a'`/`one'` material。
- 新增 `fixed_runtime_demo`，只載入 `a'`、`one'`、fixed `GC_f`，不載入 `hpk/hsk`，輸出 predicate sequence `1,1,1,1,1,0`。
- 新增 `active_garbled_circuit_io` 與 `boolean_circuit_io`，讓 evaluator runtime 載入 serialized GC artifact 和 redacted circuit shape。
- 新增 OpenFHE fixed material：`a_prime_bit_0..3`、`b_prime_bit_0..3`、`one_prime_integer_bit_0..3`。
- 新增 `evaluator_runtime_material_check`，只載入 OpenFHE context/evaluation keys、`a'`、integer `one'`，做一次 encrypted add，確認 runtime 不需 `hpk/hsk`。
- 新增 `OpenFHEEvalBinGateCircuit`，把 demo LWE ciphertext fields、OpenFHE additive pre-bootstrap step、固定 demo `hsk` selected-label constants 展開成 Boolean circuit。
- 新增 `dump_openfhe_eval_bin_gate_circuit` 與 `./build_mock.sh --evalbingate-circuit`，可輸出 AND/OR/XOR/XNOR 的 `openfhe_evalbingate_*_demo.txt` circuit dump。
- 新增 `src/gc/openfhe_controlled_reveal_reference.cpp`，用單檔 lowering-ready logic 明確寫出 `Dec_hsk(OpenFHE.Eval([x <= b], x', b'))`，沒有 `main`、include、OpenFHE runtime type、path、print 或 key-loading dependency。
- 把 lowering source 的 fixed hsk 換成已生成 OpenFHE keypair 的實際 ternary secret，並對齊目前 demo material 的 `n=64, q=512`。
- 移除 lowering source 裡「先解出 gate bit 再假重加密」的 semantic shortcut，改成 `BootstrapGateCoreOpenFHE -> EvalAccCGGI -> SwitchCTtoqn` 的 OpenFHE gate/bootstrap 資料流骨架。
- 新增 `openfhe_controlled_reveal_reference_test`，不 link OpenFHE，直接用實際 OpenFHE `Enc(1)` / `Enc(0)` ciphertext 驗證 fixed-hsk decryption arithmetic。
- 新增 `export_openfhe_lwe_int_material`，用現有 OpenFHE `hpk/hsk` 直接產生單一 4-bit integer LWE ciphertext material：`a'=Enc(3)`、`b'=Enc(7)`、`one'=Enc(1)`，並匯出 `a` vector、`b` body、`q`、`p`、`hsk.s_mod_q`。
- 新增 `test_openfhe_lwe_int_material_export.sh`，驗證 OpenFHE Decrypt 與 exported-field `phase = b - <a,s> mod q`、`round_p(phase)` 手寫 Dec 一致。
- 新增 `test_controlled_reveal_source_no_placeholder.sh`，防止 lowering source 又退回 semantic gate decode 或 fake re-encrypt。
- 新增 `export_openfhe_eval_key_material`，可把已生成的 OpenFHE `eval_refresh_key.bin` / `eval_switch_key.bin` 匯出成 plain C++ integer arrays，供後續 standalone `.cpp` / GC lowering 使用。
- 實測固定 demo eval-key material 大小：refresh key `524288` words；switch key A `4915200` words；switch key B `76800` words。產生的 C++ material 約 `70MB`。
- 對齊 OpenFHE `BootstrapGateCore` 的 gate constants 與 accumulator sparse LUT 初始化：`AND=448`、`XOR=384`，使用 `[lb, ub)` range 和 `Q/(2p)+1` / `Q-Q/(2p)-1` message mapping。
- 新增 CGGI bootstrap primitives：signed digit decomposition、negacyclic monomial multiplication、negacyclic polynomial multiplication、coefficient-domain external product。

## 還差什麼

- 把 `src/gc/openfhe_controlled_reveal_reference.cpp` 的完整邏輯 lowering 成 Boolean circuit。
- 把目前 transitional fixed-bound `GC_f(x')` runtime 改回最終需要的 `GC_f(x', b')` runtime contract。
- 把 `OpenFHEEvalBinGateCircuit` dump 裡的 `openfhe_bootstrap_placeholder` 換成真正 OpenFHE `BootstrapGateCore` 展開；目前 standalone source 已對齊 gate constants / coefficient-domain accumulator init，但 dump 工具仍只展開到 LWE additive pre-bootstrap 與 demo LUT placeholder。
- 把 `openfhe_controlled_reveal_reference.cpp` 裡的 `ExternalProductCGGI`、`EvalAccCGGI`、`SwitchCTtoqn` 補成 OpenFHE 1.5.0 bit-accurate arithmetic；目前已移除 semantic shortcut，但 bootstrap/key-switch internals 仍是結構骨架。
- 把 `export_openfhe_eval_key_material` 產生的 flat words 精確接回 `EvalAccCGGI` / `SwitchCTtoqn` 的索引 layout。
- 補 OpenFHE NTT/evaluation-domain 對齊，或把 exporter 改成輸出 coefficient-domain eval keys 並讓 source 全程使用 coefficient-domain convolution。
- 對齊 production-security OpenFHE 參數、ciphertext fields 和 noise range；目前 lowering source 使用實際 OpenFHE TOY key/ciphertext material，仍不是安全參數。
- 決定真實 ciphertext serialization 格式，讓 `f(x)'` 的 `a` vector 和 `b` body 能被 GC decryption circuit 讀入。
- 把 `export_openfhe_lwe_int_material` 的真實 OpenFHE integer ciphertext field bits 接到 v2 `GC{[Dec(x') <= Dec(b')]}` input，而不是目前 fixed runtime 的 mock `.bit` material。
- 把真實 OpenFHE ciphertext bit/word serialization 接到 GC input，而不是目前的 `MOCK_OPENFHE` `.bit`。
- 把實際 OpenFHE `hsk.s_mod_q` 展成 v2 `Circuit_g` secret selected-label material；目前 exporter 已能輸出 setup-side hsk words，但主要 GC runtime 仍使用早期 toy 4-bit hsk。
- 把 `b'` 的真實 OpenFHE ciphertext/eval material 固定進 GC，而不是目前用 fixed plaintext bound bits 做 mock comparator。
- 若要 production security，需要把 `TOY` 參數換成安全參數並重新評估 circuit/gate size。
- 加測試：comparator correctness、increment correctness、BooleanCircuit evaluation、EMP GC label flow。

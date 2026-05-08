# TODO

## 已完成

- 移除 ABE2 selector prototype，專案方向改成 FHE comparison algorithm + GC controlled reveal。
- 建立 `ControlledRevealCircuit`，把 `FHE.Eval([x <= b])` 和只解密 predicate bit 包成同一個介面。
- 新增 `EncryptedPredicateEvaluator`，讓 evaluator loop 只依賴 `GC_f(x', b') -> bool`。
- 新增 encrypted add，demo 可以跑 encrypted loop：`x' = a'`，每輪 `x' <- FHE.Add(x', one')`，其中 `one'=Enc(1)` 由 client 提供。
- 新增 `BooleanCircuit`，用 stable `WireId`、`GateId`、input wires、output wires 描述 `Circuit_g`。
- 新增 minimal GC backend，產生 wire labels、garbled tables，並用 label flow 做 mock evaluation。
- 新增 `GarbledPredicateEvaluator`，讓 mock demo loop 每一輪都直接 evaluate `Circuit_g` garbled artifact。
- 新增 OpenFHE LWE-like decryption circuit：固定 `hsk=[1,0,1,1]`、`a=[3,5,6,1]`、`q=16`，展開 `b - <a,hsk> mod q` 和 bit decode。
- 新增 EMP-toolkit half-gates backend，Docker `gc_mock` 會安裝 EMP 並用 `USE_EMP_GC=1` 真實執行 EMP GC path。
- 新增 `artifacts/circuit_g_demo.txt`，不用只靠 runtime stdout 也能檢查 `FHE.Dec(hsk,FHE.Eval(f,x'))` 的 Boolean circuit gate list。
- 新增 `demo_keys/future_demo_fhe_key_material.json`，記錄目前 demo 使用的固定 hsk 與 `Enc(1)` 設計決策。
- 新增 `gc.md`，用中文說明 controlled reveal、circuit 描述、EMP GC 和 demo leakage model。

## 還差什麼

- 對齊 OpenFHE production decrypt 的 rounding/noise handling，而不是目前 noiseless `phase[2]` decode。
- 決定真實 ciphertext serialization 格式，讓 `f(x)'` 的 `a` vector 和 `b` body 能被 GC decryption circuit 讀入。
- 設計 serialization：client 產生 `GC_f` artifact，evaluator 從檔案載入。
- 把真實 OpenFHE ciphertext bit/word serialization 接到 GC input，而不是目前的 `MOCK_OPENFHE` `.bit`。
- 若 production 需要真正 public-key `hpk/hsk`，需要切到 OpenFHE PKE scheme 或確認 BinFHE public-key support；目前 BinFHE prototype 沒有 hpk serialization。
- 把 demo 拆成 client setup 和 evaluator loop 兩個 executable。
- 加測試：comparator correctness、increment correctness、BooleanCircuit evaluation、EMP GC label flow。

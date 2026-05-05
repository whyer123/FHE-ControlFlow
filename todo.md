# TODO

## 已完成

- 移除 ABE2 selector prototype，專案方向改成 FHE comparison algorithm + GC controlled reveal。
- 建立 `ControlledRevealCircuit`，把 `FHE.Eval([x <= b])` 和只解密 predicate bit 包成同一個介面。
- 新增 `EncryptedPredicateEvaluator`，讓 evaluator loop 只依賴 `GC_f(x', b') -> bool`。
- 新增 bit-level homomorphic increment，demo 可以跑 encrypted loop：`x' = a'`，每輪 `x' <- x' + 1'`。
- 新增 `BooleanCircuit`，用 stable `WireId`、`GateId`、input wires、output wires 描述 `Circuit_g`。
- 新增 minimal GC backend，產生 wire labels、garbled tables，並用 label flow 做 mock evaluation。
- 新增 fixed-hsk mock decryption stage：固定 `hsk=[1,0,1,1]`，用 `<mask,hsk>` parity 展開 `Dec(hsk, f(x)')` 的 Boolean circuit 形狀。
- 新增 `gc.md`，用中文說明 controlled reveal、circuit 描述、minimal GC 和 demo leakage model。

## 還差什麼

- 把 toy mock decryption 換成真正 OpenFHE/LWE ciphertext 的 decryption Boolean circuit。
- 決定真實 ciphertext serialization 格式，讓 `f(x)'` 能被 GC decryption circuit 讀入。
- 設計 serialization：client 產生 `GC_f` artifact，evaluator 從檔案載入。
- 把 demo 拆成 client setup 和 evaluator loop 兩個 executable。
- 加測試：comparator correctness、increment correctness、BooleanCircuit evaluation、minimal GC label flow。

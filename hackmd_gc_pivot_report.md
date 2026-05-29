# GC 內部電路設計調整結論

本階段建議將 GC 內部電路由：

$$
\begin{array}{l}
GC_f^{old}(x', b') = GC\{ \\
\quad
    \operatorname{Dec}_{hsk}
    \left(
        \operatorname{FHE.Eval}([x \le b], x', b')
    \right) \\
\}
\end{array}
$$

改為：

$$
\begin{array}{l}
GC_f^{new}(x', b') = GC\{ \\
\quad
    \left[
        \operatorname{Dec}_{hsk}(x')
        \le
        \operatorname{Dec}_{hsk}(b')
    \right] \\
\}
\end{array}
$$

這個改動保留第一版 demo 需要的受限揭露語意：

$$
GC_f(x',b') \rightarrow [x \le b]
$$

同時避免把完整 OpenFHE homomorphic gate evaluation 展開成 GC，讓 prototype 可以落在實際可實作與可展示的範圍內。

## 需要注意的攻擊

我發現如果 evaluator 拿得到可重複使用的：

$$
1'=\operatorname{Enc}(1)
$$

並且可以自由呼叫比較型 GC，那他可以從原資料：

$$
a'
$$

開始反覆做同態減法：

$$
a',\ a'-1',\ a'-2',\ldots
$$

再用 GC 檢查目前值是否仍然 greater than 某個固定值，例如：

$$
GC(a'-t\cdot 1', 1')
$$

當比較結果第一次改變時，evaluator 就可以由迭代次數 \(t\) 推回原本的 \(a\)。因此，若 GC 可以被重複查詢，而且 evaluator 可以自由更新 ciphertext，單純不給公鑰仍然不夠，需要限制可查詢的 predicate 或限制 `1'` 的使用方式。

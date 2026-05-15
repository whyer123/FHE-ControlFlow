#pragma once

#include "binfhecontext.h"

#include <stdexcept>
#include <vector>

namespace controlled_reveal_reference {

// 這個檔案故意寫成 header-only inline logic，而不是 main.cpp。
// 原因是下一步要把同一段「純邏輯」lowering 成 Boolean circuit：
// 呼叫端會把 hsk 固定成 GC 內的 secret constants / selected labels，
// 並把 x'、b' 的 ciphertext fields 接成 circuit input wires。
// 因此這裡不能讀檔、不能從 path 載入私鑰、不能 print runtime 資訊。

inline lbcrypto::LWECiphertext EvalLessOrEqualPredicate(
    lbcrypto::BinFHEContext& cc,
    const std::vector<lbcrypto::LWECiphertext>& x,
    const std::vector<lbcrypto::LWECiphertext>& bound) {
    if (x.empty()) {
        throw std::invalid_argument("predicate inputs must be non-empty.");
    }
    if (x.size() != bound.size()) {
        throw std::invalid_argument(
            "predicate inputs must have equal bit widths.");
    }

    // 這個函式就是：
    //
    //     OpenFHE.Eval([x <= b], x', b')
    //
    // x 和 bound 都是 bit-sliced LWE ciphertext array：
    //   x[i]     = x' 的第 i 個 encrypted bit
    //   bound[i] = b' 的第 i 個 encrypted bit
    //
    // 下面用 OpenFHE EvalNOT / EvalBinGate 建出 encrypted comparator。
    auto not_bound0 = cc.EvalNOT(bound[0]);
    lbcrypto::LWECiphertext greater =
        cc.EvalBinGate(lbcrypto::AND, not_bound0, x[0]);

    for (size_t i = 1; i < x.size(); ++i) {
        auto not_bound_i = cc.EvalNOT(bound[i]);
        auto generate = cc.EvalBinGate(lbcrypto::AND, not_bound_i, x[i]);
        auto xor_bits = cc.EvalBinGate(lbcrypto::XOR, bound[i], x[i]);
        auto equal_bits = cc.EvalNOT(xor_bits);
        auto propagate = cc.EvalBinGate(lbcrypto::AND, equal_bits, greater);

        // generate and propagate are mutually exclusive in this comparator.
        greater = cc.EvalBinGate(lbcrypto::XOR, generate, propagate);
    }

    return cc.EvalNOT(greater);
}

inline bool DecOfEvalLessOrEqual(
    lbcrypto::BinFHEContext& cc,
    const lbcrypto::LWEPrivateKey& hsk,
    const std::vector<lbcrypto::LWECiphertext>& x,
    const std::vector<lbcrypto::LWECiphertext>& bound) {
    // 這個函式就是完整目標：
    //
    //     Dec_hsk(OpenFHE.Eval([x <= b], x', b'))
    //
    // 私鑰 hsk 在這裡不是從 path 讀進來，而是明確作為參數傳入。
    // 之後轉成 GC 時，這個 hsk 參數會被固定寫進 GC：
    // evaluator 只會拿到對應 hsk bits 的 selected labels，不會拿到 hsk 明文。
    const auto predicate_ciphertext = EvalLessOrEqualPredicate(cc, x, bound);

    // 這一行對應 Dec_hsk(...)，而且只解密上一行產生的 predicate ciphertext。
    // 它不是 generic Dec_hsk(c') oracle，不能拿任意 ciphertext 進來解密。
    lbcrypto::LWEPlaintext predicate_bit = 0;
    cc.Decrypt(hsk, predicate_ciphertext, &predicate_bit);
    return predicate_bit == 1;
}

} // namespace controlled_reveal_reference

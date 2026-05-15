// 單檔 reference logic：
//
//     Dec_hsk(OpenFHE.Eval([x <= b], x', b'))
//
// 這個檔案故意把之後要 lowering 成 Boolean circuit 的邏輯集中在同一處。
// 不在這裡 keygen、讀 path、讀檔、serialization 或 print，因為那些都不是
// GC circuit 的一部分。

#include "binfhecontext.h"

#include <stdexcept>
#include <vector>

namespace controlled_reveal_reference {

using lbcrypto::BinFHEContext;
using lbcrypto::LWEPlaintext;
using lbcrypto::LWEPrivateKey;
using lbcrypto::LWECiphertext;

void RequireSameNonEmptyWidth(const std::vector<LWECiphertext>& x,
                              const std::vector<LWECiphertext>& bound) {
    if (x.empty()) {
        throw std::invalid_argument("predicate inputs must be non-empty.");
    }
    if (x.size() != bound.size()) {
        throw std::invalid_argument(
            "predicate inputs must have equal bit widths.");
    }
}

LWECiphertext EvalAnd(BinFHEContext& cc,
                      const LWECiphertext& lhs,
                      const LWECiphertext& rhs) {
    return cc.EvalBinGate(lbcrypto::AND, lhs, rhs);
}

LWECiphertext EvalXor(BinFHEContext& cc,
                      const LWECiphertext& lhs,
                      const LWECiphertext& rhs) {
    return cc.EvalBinGate(lbcrypto::XOR, lhs, rhs);
}

LWECiphertext EvalNot(BinFHEContext& cc, const LWECiphertext& value) {
    return cc.EvalNOT(value);
}

LWECiphertext EvalLessOrEqualPredicate(
    BinFHEContext& cc,
    const std::vector<LWECiphertext>& x,
    const std::vector<LWECiphertext>& bound) {
    RequireSameNonEmptyWidth(x, bound);

    // 這段就是：
    //
    //     OpenFHE.Eval([x <= b], x', b')
    //
    // x 和 bound 都是 bit-sliced LWE ciphertext array：
    //   x[i]     = x' 的第 i 個 encrypted bit
    //   bound[i] = b' 的第 i 個 encrypted bit
    //
    // comparator 實作策略：
    //   greater = [x > b]
    //   result  = NOT(greater) = [x <= b]
    //
    // 每個 AND/XOR/NOT 都是 OpenFHE homomorphic gate evaluation。
    auto not_bound0 = EvalNot(cc, bound[0]);
    LWECiphertext greater = EvalAnd(cc, not_bound0, x[0]);

    for (size_t i = 1; i < x.size(); ++i) {
        auto not_bound_i = EvalNot(cc, bound[i]);
        auto generate = EvalAnd(cc, not_bound_i, x[i]);
        auto xor_bits = EvalXor(cc, bound[i], x[i]);
        auto equal_bits = EvalNot(cc, xor_bits);
        auto propagate = EvalAnd(cc, equal_bits, greater);

        // generate 和 propagate 在這個 comparator 中互斥，所以 OR 可用 XOR。
        greater = EvalXor(cc, generate, propagate);
    }

    return EvalNot(cc, greater);
}

bool DecOfEvalLessOrEqual(
    BinFHEContext& cc,
    const LWEPrivateKey& hsk,
    const std::vector<LWECiphertext>& x,
    const std::vector<LWECiphertext>& bound) {
    // 這個函式就是完整目標：
    //
    //     Dec_hsk(OpenFHE.Eval([x <= b], x', b'))
    //
    // 私鑰在這裡：
    //
    //     const LWEPrivateKey& hsk
    //
    // hsk 不是從 path 載入，也不是 runtime evaluator 會拿到的東西。
    // 之後轉成 GC 時，這個參數會變成固定 secret constants /
    // selected labels，寫進 GC artifact。
    const auto predicate_ciphertext = EvalLessOrEqualPredicate(cc, x, bound);

    // 這一行就是 Dec_hsk(...)。
    // 它只解密上一行產生的 predicate ciphertext，不提供 generic Dec(c')。
    LWEPlaintext predicate_bit = 0;
    cc.Decrypt(hsk, predicate_ciphertext, &predicate_bit);
    return predicate_bit == 1;
}

std::vector<LWECiphertext> AddEncryptedIntegerBits(
    BinFHEContext& cc,
    const std::vector<LWECiphertext>& lhs,
    const std::vector<LWECiphertext>& rhs) {
    if (lhs.size() != rhs.size()) {
        throw std::invalid_argument("encrypted add expects equal bit widths.");
    }
    if (lhs.empty()) {
        throw std::invalid_argument("encrypted add expects non-empty inputs.");
    }

    // 這是 loop runtime 的 x' <- x' + one'，不是 GC_f 的一部分。
    // 放在同一檔是為了讓目前 controlled loop reference 不用跳到其他檔案看。
    std::vector<LWECiphertext> result(lhs.size());
    auto xor_bits = EvalXor(cc, lhs[0], rhs[0]);
    result[0] = xor_bits;
    auto carry = EvalAnd(cc, lhs[0], rhs[0]);

    for (size_t i = 1; i < lhs.size(); ++i) {
        xor_bits = EvalXor(cc, lhs[i], rhs[i]);
        result[i] = EvalXor(cc, xor_bits, carry);
        const auto generate = EvalAnd(cc, lhs[i], rhs[i]);
        const auto propagate = EvalAnd(cc, xor_bits, carry);

        // generate 和 propagate 在 full adder 中互斥，所以 OR 可用 XOR。
        carry = EvalXor(cc, generate, propagate);
    }

    return result;
}

} // namespace controlled_reveal_reference

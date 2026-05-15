#pragma once

#include "binfhecontext.h"

#include <stdexcept>
#include <vector>

namespace controlled_reveal_reference {

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
    const auto predicate_ciphertext = EvalLessOrEqualPredicate(cc, x, bound);

    lbcrypto::LWEPlaintext predicate_bit = 0;
    cc.Decrypt(hsk, predicate_ciphertext, &predicate_bit);
    return predicate_bit == 1;
}

} // namespace controlled_reveal_reference

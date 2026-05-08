#include "fhe_arithmetic.h"
#include <stdexcept>

std::vector<LWECiphertext> FHEArithmetic::Increment(
    const std::vector<LWECiphertext>& x) {
    if (x.empty()) {
        return {};
    }

    std::vector<LWECiphertext> result(x.size());
    LWECiphertext carry = x[0];
    result[0] = fhe_gates.EvalNOT(x[0]);

    for (size_t i = 1; i < x.size(); ++i) {
        result[i] = fhe_gates.EvalXOR(x[i], carry);
        carry = fhe_gates.EvalAND(x[i], carry);
    }

    return result;
}

std::vector<LWECiphertext> FHEArithmetic::Add(
    const std::vector<LWECiphertext>& lhs,
    const std::vector<LWECiphertext>& rhs) {
    if (lhs.size() != rhs.size()) {
        throw std::invalid_argument("Encrypted add expects equal bit widths.");
    }
    if (lhs.empty()) {
        return {};
    }

    std::vector<LWECiphertext> result(lhs.size());

    auto xor_bits = fhe_gates.EvalXOR(lhs[0], rhs[0]);
    result[0] = xor_bits;
    auto carry = fhe_gates.EvalAND(lhs[0], rhs[0]);

    for (size_t i = 1; i < lhs.size(); ++i) {
        xor_bits = fhe_gates.EvalXOR(lhs[i], rhs[i]);
        result[i] = fhe_gates.EvalXOR(xor_bits, carry);

        const auto generate = fhe_gates.EvalAND(lhs[i], rhs[i]);
        const auto propagate = fhe_gates.EvalAND(xor_bits, carry);
        carry = fhe_gates.EvalXOR(generate, propagate);
    }

    return result;
}

#include "fhe_arithmetic.h"

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

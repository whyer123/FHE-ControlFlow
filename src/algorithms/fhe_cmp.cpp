#include "fhe_cmp.h"

LWECiphertext FHECompare::EqualToZero(const std::vector<LWECiphertext>& x) {
    if (x.empty()) return LWECiphertext();
    LWECiphertext result = fhe_gates.EvalNOT(x[0]);
    for (size_t i = 1; i < x.size(); ++i) {
        auto not_xi = fhe_gates.EvalNOT(x[i]);
        result = fhe_gates.EvalAND(result, not_xi);
    }
    return result;
}

LWECiphertext FHECompare::Equal(const std::vector<LWECiphertext>& x, const std::vector<LWECiphertext>& y) {
    if (x.empty() || x.size() != y.size()) return LWECiphertext();
    LWECiphertext result = fhe_gates.EvalXNOR(x[0], y[0]);
    for (size_t i = 1; i < x.size(); ++i) {
        auto xnor_i = fhe_gates.EvalXNOR(x[i], y[i]);
        result = fhe_gates.EvalAND(result, xnor_i);
    }
    return result;
}

LWECiphertext FHECompare::GreaterThan(const std::vector<LWECiphertext>& x, const std::vector<LWECiphertext>& y) {
    if (x.empty() || x.size() != y.size()) return LWECiphertext();
    
    // We will accumulate the result from LSB to MSB.
    auto not_y0 = fhe_gates.EvalNOT(y[0]);
    LWECiphertext result = fhe_gates.EvalAND(x[0], not_y0); // gt_0
    
    for (size_t i = 1; i < x.size(); ++i) {
        auto not_yi = fhe_gates.EvalNOT(y[i]);
        auto gt_i = fhe_gates.EvalAND(x[i], not_yi);
        auto eq_i = fhe_gates.EvalXNOR(x[i], y[i]);
        
        auto tmp = fhe_gates.EvalAND(eq_i, result);
        result = fhe_gates.EvalOR(gt_i, tmp);
    }
    return result;
}

LWECiphertext FHECompare::LessThan(const std::vector<LWECiphertext>& x, const std::vector<LWECiphertext>& y) {
    return GreaterThan(y, x);
}

std::vector<LWECiphertext> FHECompare::FindMax(const std::vector<LWECiphertext>& x, const std::vector<LWECiphertext>& y) {
    auto sel = GreaterThan(x, y);
    std::vector<LWECiphertext> result(x.size());
    for (size_t i = 0; i < x.size(); ++i) {
        result[i] = fhe_gates.EvalMUX(sel, x[i], y[i]);
    }
    return result;
}

std::vector<LWECiphertext> FHECompare::FindMin(const std::vector<LWECiphertext>& x, const std::vector<LWECiphertext>& y) {
    auto sel = LessThan(x, y);
    std::vector<LWECiphertext> result(x.size());
    for (size_t i = 0; i < x.size(); ++i) {
        result[i] = fhe_gates.EvalMUX(sel, x[i], y[i]);
    }
    return result;
}

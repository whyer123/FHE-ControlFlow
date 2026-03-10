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
    
    // x > y is equivalent to y < x.
    // We can use a ripple-borrow subtractor for (y - x). 
    // If y < x, the final borrow-out will be 1.
    // borrow_out = NOT(y) AND x (for bit 0)
    // borrow_i = (NOT(y_i) AND x_i) OR (NOT(y_i XOR x_i) AND borrow_{i-1})
    
    auto not_y0 = fhe_gates.EvalNOT(y[0]);
    LWECiphertext borrow = fhe_gates.EvalAND(not_y0, x[0]);
    
    for (size_t i = 1; i < x.size(); ++i) {
        auto not_yi = fhe_gates.EvalNOT(y[i]);
        auto gen = fhe_gates.EvalAND(not_yi, x[i]); // Generate borrow: y_i=0, x_i=1
        
        auto eq = fhe_gates.EvalXNOR(y[i], x[i]);   // Propagate borrow: y_i == x_i
        auto prop = fhe_gates.EvalAND(eq, borrow);
        
        borrow = fhe_gates.EvalOR(gen, prop);
    }
    return borrow; // If y < x, final borrow is 1, meaning x > y.
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

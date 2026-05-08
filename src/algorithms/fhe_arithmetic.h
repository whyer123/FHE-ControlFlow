#pragma once

#include "src/gates/fhe_gates.h"
#include <vector>

class FHEArithmetic {
public:
    explicit FHEArithmetic(FHEGates& gates) : fhe_gates(gates) {}

    std::vector<LWECiphertext> Increment(
        const std::vector<LWECiphertext>& x);

    std::vector<LWECiphertext> Add(
        const std::vector<LWECiphertext>& lhs,
        const std::vector<LWECiphertext>& rhs);

private:
    FHEGates& fhe_gates;
};

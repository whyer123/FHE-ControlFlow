#pragma once

#include "src/gates/fhe_gates.h"
#include <vector>

class FHEArithmetic {
public:
    explicit FHEArithmetic(FHEGates& gates) : fhe_gates(gates) {}

    std::vector<LWECiphertext> Increment(
        const std::vector<LWECiphertext>& x);

private:
    FHEGates& fhe_gates;
};

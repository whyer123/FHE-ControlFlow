#pragma once

#include "src/gc/boolean_circuit.h"

#include <string>
#include <vector>

enum class OpenFHEBinGateKind {
    And,
    Or,
    Xor,
    Xnor
};

struct DemoLWECiphertextWireBits {
    std::vector<std::vector<WireId>> mask_coefficients;
    std::vector<WireId> body_bits;
};

class OpenFHEEvalBinGateCircuit {
public:
    static BooleanCircuit DescribeDemoEvalBinGate(OpenFHEBinGateKind gate);

    static DemoLWECiphertextWireBits AddDemoCiphertextInput(
        BooleanCircuitBuilder& builder,
        const std::string& prefix);

    static DemoLWECiphertextWireBits BuildDemoEvalBinGate(
        BooleanCircuitBuilder& builder,
        OpenFHEBinGateKind gate,
        const DemoLWECiphertextWireBits& lhs,
        const DemoLWECiphertextWireBits& rhs,
        const std::string& prefix);
};

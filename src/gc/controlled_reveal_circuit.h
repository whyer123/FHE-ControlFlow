#pragma once

#include "src/fhe/fhe_context.h"
#include "src/gates/fhe_gates.h"
#include <cstddef>
#include <string>
#include <vector>

enum class BitGateKind {
    And,
    Xor,
    Not,
    Mux,
    Output
};

struct BitGate {
    BitGateKind kind;
    std::vector<std::string> inputs;
    std::string output;
};

struct BitLevelCircuit {
    std::string name;
    size_t input_bit_length = 0;
    std::vector<std::string> input_wires;
    std::vector<BitGate> gates;
    std::string output_wire;
};

std::string BitGateKindToString(BitGateKind kind);

class ControlledRevealCircuit {
public:
    ControlledRevealCircuit(FHEContextWrapper& ctx, FHEGates& gates)
        : fhe_ctx(ctx), fhe_gates(gates) {}

    LWECiphertext EvalLessOrEqualPredicate(
        const std::vector<LWECiphertext>& x,
        const std::vector<LWECiphertext>& bound);

    bool RevealPredicateOnly(const LWECiphertext& predicate_ct);

    bool Evaluate(const std::vector<LWECiphertext>& x,
                  const std::vector<LWECiphertext>& bound);

    BitLevelCircuit DescribeLessOrEqualCircuit(size_t bit_length) const;

private:
    FHEContextWrapper& fhe_ctx;
    FHEGates& fhe_gates;
};

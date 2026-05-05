#pragma once

#include "src/gc/boolean_circuit.h"
#include "src/gc/predicate_gc.h"
#include "src/fhe/fhe_context.h"
#include "src/gates/fhe_gates.h"
#include <cstddef>
#include <vector>

class ControlledRevealCircuit : public EncryptedPredicateEvaluator {
public:
    ControlledRevealCircuit(FHEContextWrapper& ctx, FHEGates& gates)
        : fhe_ctx(ctx), fhe_gates(gates) {}

    LWECiphertext EvalLessOrEqualPredicate(
        const std::vector<LWECiphertext>& x,
        const std::vector<LWECiphertext>& bound);

    bool RevealPredicateOnly(const LWECiphertext& predicate_ct);

    bool Evaluate(const std::vector<LWECiphertext>& x,
                  const std::vector<LWECiphertext>& bound) override;

    PredicateGCArtifact ArtifactInfo(size_t bit_length) const override;

    BooleanCircuit DescribeLessOrEqualCircuit(size_t bit_length) const;

private:
    FHEContextWrapper& fhe_ctx;
    FHEGates& fhe_gates;
};

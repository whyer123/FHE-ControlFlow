#pragma once

#include "src/gc/active_garbled_circuit.h"
#include "src/gc/predicate_gc.h"

class GarbledPredicateEvaluator : public EncryptedPredicateEvaluator {
public:
    GarbledPredicateEvaluator(BooleanCircuit circuit,
                              ActiveGarbledCircuitArtifact artifact);

    bool Evaluate(const std::vector<LWECiphertext>& x,
                  const std::vector<LWECiphertext>& bound) override;

    bool EvaluateFixedBound(const std::vector<LWECiphertext>& x);

    PredicateGCArtifact ArtifactInfo(size_t bit_length) const override;

private:
    BooleanCircuit circuit_;
    ActiveGarbledCircuitArtifact artifact_;
};

#pragma once

#ifdef USE_EMP_GC
#include "src/gc/emp_garbled_circuit.h"
using ActiveGarbledCircuitArtifact = EmpGarbledCircuitArtifact;
#else
#include "src/gc/minimal_garbled_circuit.h"
using ActiveGarbledCircuitArtifact = GarbledCircuitArtifact;
#endif

#include "src/gc/predicate_gc.h"

class GarbledPredicateEvaluator : public EncryptedPredicateEvaluator {
public:
    GarbledPredicateEvaluator(BooleanCircuit circuit,
                              ActiveGarbledCircuitArtifact artifact);

    bool Evaluate(const std::vector<LWECiphertext>& x,
                  const std::vector<LWECiphertext>& bound) override;

    PredicateGCArtifact ArtifactInfo(size_t bit_length) const override;

private:
    BooleanCircuit circuit_;
    ActiveGarbledCircuitArtifact artifact_;
};

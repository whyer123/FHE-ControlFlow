#pragma once

#include "src/fhe/fhe_context.h"
#include <string>
#include <vector>

struct PredicateGCArtifact {
    std::string name;
    size_t input_bit_length = 0;
    size_t gate_count = 0;
};

class EncryptedPredicateEvaluator {
public:
    virtual ~EncryptedPredicateEvaluator() = default;

    virtual bool Evaluate(const std::vector<LWECiphertext>& x,
                          const std::vector<LWECiphertext>& bound) = 0;

    virtual PredicateGCArtifact ArtifactInfo(size_t bit_length) const = 0;
};

#include "garbled_predicate_evaluator.h"
#include <stdexcept>
#include <unordered_map>
#include <utility>

GarbledPredicateEvaluator::GarbledPredicateEvaluator(
    BooleanCircuit circuit,
    ActiveGarbledCircuitArtifact artifact)
    : circuit_(std::move(circuit)), artifact_(std::move(artifact)) {}

bool GarbledPredicateEvaluator::Evaluate(
    const std::vector<LWECiphertext>& x,
    const std::vector<LWECiphertext>& bound) {
    if (x.size() != circuit_.input_bit_length ||
        bound.size() != circuit_.input_bit_length) {
        throw std::invalid_argument("GC input bit length does not match the circuit.");
    }
    if (circuit_.input_wires.size() != circuit_.input_bit_length * 2) {
        throw std::invalid_argument("Predicate circuit must expose x_i,b_i input pairs.");
    }

#if defined(MOCK_OPENFHE)
    std::unordered_map<WireId, bool> input_bits;
    for (size_t i = 0; i < circuit_.input_bit_length; ++i) {
        input_bits[circuit_.input_wires[2 * i]] = x[i].bit;
        input_bits[circuit_.input_wires[2 * i + 1]] = bound[i].bit;
    }

    auto decoded_outputs = EvaluateActiveCircuit(artifact_, circuit_, input_bits);

    if (decoded_outputs.size() != 1) {
        throw std::invalid_argument("Predicate GC must produce exactly one output bit.");
    }
    return decoded_outputs.front();
#else
    (void)x;
    (void)bound;
    throw std::runtime_error(
        "GarbledPredicateEvaluator needs serialized ciphertext-bit inputs; "
        "currently implemented for MOCK_OPENFHE only.");
#endif
}

bool GarbledPredicateEvaluator::EvaluateFixedBound(
    const std::vector<LWECiphertext>& x) {
    if (x.size() != circuit_.input_bit_length) {
        throw std::invalid_argument("GC input bit length does not match the circuit.");
    }
    if (circuit_.input_wires.size() != circuit_.input_bit_length) {
        throw std::invalid_argument(
            "Fixed-bound predicate circuit must expose only x_i input wires.");
    }

#if defined(MOCK_OPENFHE)
    std::unordered_map<WireId, bool> input_bits;
    for (size_t i = 0; i < circuit_.input_bit_length; ++i) {
        input_bits[circuit_.input_wires[i]] = x[i].bit;
    }

    auto decoded_outputs = EvaluateActiveCircuit(artifact_, circuit_, input_bits);

    if (decoded_outputs.size() != 1) {
        throw std::invalid_argument("Predicate GC must produce exactly one output bit.");
    }
    return decoded_outputs.front();
#else
    (void)x;
    throw std::runtime_error(
        "Fixed-bound GarbledPredicateEvaluator needs serialized ciphertext-bit "
        "inputs; currently implemented for MOCK_OPENFHE only.");
#endif
}

PredicateGCArtifact GarbledPredicateEvaluator::ArtifactInfo(size_t bit_length) const {
    if (bit_length != circuit_.input_bit_length) {
        throw std::invalid_argument("ArtifactInfo bit length does not match the circuit.");
    }

    return {circuit_.name, circuit_.input_bit_length, circuit_.gates.size()};
}

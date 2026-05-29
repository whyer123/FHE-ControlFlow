#include "src/gc/openfhe_lwe_int_decrypt_compare_circuit.h"
#include "src/gc/openfhe_lwe_int_material.h"

#ifdef USE_EMP_GC
#include "src/gc/emp_garbled_circuit.h"
#else
#include "src/gc/minimal_garbled_circuit.h"
#endif

#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

namespace {

using BitMap = std::unordered_map<WireId, bool>;

void Require(bool condition, const std::string& message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

std::unordered_map<std::string, WireId> WireNameMap(
    const BooleanCircuit& circuit) {
    std::unordered_map<std::string, WireId> map;
    for (const auto& wire : circuit.wires) {
        map.emplace(wire.name, wire.id);
    }
    return map;
}

void SetBits(BitMap& values,
             const std::unordered_map<std::string, WireId>& wires,
             const std::string& name,
             uint64_t value,
             size_t width) {
    for (size_t bit = 0; bit < width; ++bit) {
        const auto it = wires.find(name + "_bit_" + std::to_string(bit));
        Require(it != wires.end(), "missing wire " + name);
        values[it->second] = ((value >> bit) & 1ULL) != 0;
    }
}

void SetCiphertext(BitMap& values,
                   const BooleanCircuit& circuit,
                   const std::string& prefix,
                   const OpenFHELWEIntCiphertextMaterial& ciphertext,
                   size_t modulus_bits) {
    const auto wires = WireNameMap(circuit);
    for (size_t i = 0; i < ciphertext.a.size(); ++i) {
        SetBits(values, wires, prefix + "_a_" + std::to_string(i),
                ciphertext.a.at(i), modulus_bits);
    }
    SetBits(values, wires, prefix + "_body", ciphertext.body, modulus_bits);
}

BitMap BuildInputBits(const BooleanCircuit& circuit,
                      const OpenFHELWEIntCiphertextMaterial& lhs,
                      const OpenFHELWEIntCiphertextMaterial& rhs,
                      size_t modulus_bits) {
    BitMap values;
    SetCiphertext(values, circuit, "lhs", lhs, modulus_bits);
    SetCiphertext(values, circuit, "rhs", rhs, modulus_bits);
    return values;
}

std::vector<bool> GarbleAndEvaluate(const BooleanCircuit& circuit,
                                    const BitMap& input_bits) {
#ifdef USE_EMP_GC
    EmpGarbledCircuit gc;
    auto artifact = gc.Garble(circuit);
    Require(artifact.constant_labels.size() ==
                circuit.constant_wires.size() +
                    circuit.secret_constant_wires.size(),
            "EMP artifact must include selected labels for public and secret constants.");
    return gc.Evaluate(artifact, circuit, input_bits);
#else
    MinimalGarbledCircuit gc;
    auto artifact = gc.Garble(circuit);
    Require(artifact.constant_labels.size() ==
                circuit.constant_wires.size() +
                    circuit.secret_constant_wires.size(),
            "minimal artifact must include selected labels for public and secret constants.");
    return gc.Evaluate(artifact, input_bits);
#endif
}

bool EvaluatePredicate(const BooleanCircuit& circuit,
                       const OpenFHELWEIntCiphertextMaterial& lhs,
                       const OpenFHELWEIntCiphertextMaterial& rhs,
                       size_t modulus_bits) {
    const auto outputs = GarbleAndEvaluate(
        circuit, BuildInputBits(circuit, lhs, rhs, modulus_bits));
    Require(outputs.size() == 1, "GC must output one predicate bit.");
    return outputs.front();
}

} // namespace

int main(int argc, char** argv) {
    Require(argc == 2, "usage: openfhe_lwe_int_decrypt_compare_gc_test <material.txt>");

    const auto material = ReadOpenFHELWEIntMaterial(argv[1]);
    const auto params = material.CircuitParams();
    const auto circuit = OpenFHELWEIntDecryptCompareCircuit::Describe(params);

    Require(EvaluatePredicate(circuit, material.a_prime, material.b_prime,
                              params.modulus_bits),
            "GC expected Dec(a') <= Dec(b') for OpenFHE-exported 3 <= 7");
    Require(!EvaluatePredicate(circuit, material.b_prime, material.a_prime,
                               params.modulus_bits),
            "GC expected Dec(b') <= Dec(a') to be false for 7 <= 3");
    Require(EvaluatePredicate(circuit, material.b_prime, material.b_prime,
                              params.modulus_bits),
            "GC expected Dec(b') <= Dec(b') for 7 <= 7");

#ifdef USE_EMP_GC
    std::cout << "OpenFHE LWE integer decrypt-compare EMP GC test passed.\n";
#else
    std::cout << "OpenFHE LWE integer decrypt-compare minimal GC test passed.\n";
#endif
    return EXIT_SUCCESS;
}

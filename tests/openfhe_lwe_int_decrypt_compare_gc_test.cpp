#include "src/gc/openfhe_lwe_int_decrypt_compare_circuit.h"

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

struct CiphertextFields {
    std::vector<uint64_t> a;
    uint64_t body = 0;
};

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
                   const CiphertextFields& ciphertext,
                   size_t modulus_bits) {
    const auto wires = WireNameMap(circuit);
    for (size_t i = 0; i < ciphertext.a.size(); ++i) {
        SetBits(values, wires, prefix + "_a_" + std::to_string(i),
                ciphertext.a.at(i), modulus_bits);
    }
    SetBits(values, wires, prefix + "_body", ciphertext.body, modulus_bits);
}

OpenFHELWEIntCircuitParams ExportedParams() {
    return {
        64,
        9,
        512,
        16,
        4,
        {
            511, 0, 511, 1, 1, 0, 0, 1,
            511, 511, 511, 511, 0, 511, 0, 511,
            0, 511, 511, 1, 511, 0, 0, 1,
            511, 0, 1, 1, 1, 511, 0, 511,
            511, 511, 1, 0, 0, 0, 0, 1,
            1, 1, 1, 511, 0, 1, 511, 0,
            1, 0, 0, 511, 0, 511, 1, 1,
            0, 511, 1, 511, 1, 1, 511, 0
        }
    };
}

CiphertextFields ExportedA() {
    return {{
        214, 26, 427, 34, 185, 447, 73, 23,
        189, 23, 148, 252, 501, 98, 205, 510,
        343, 389, 385, 262, 275, 464, 131, 41,
        157, 157, 445, 169, 123, 198, 370, 143,
        57, 180, 115, 46, 104, 477, 507, 135,
        130, 350, 403, 313, 8, 257, 368, 392,
        180, 267, 293, 399, 420, 385, 264, 251,
        74, 312, 12, 115, 325, 416, 421, 319
    }, 305};
}

CiphertextFields ExportedB() {
    return {{
        53, 222, 320, 349, 94, 145, 274, 22,
        371, 370, 76, 161, 376, 369, 464, 509,
        477, 81, 436, 405, 503, 311, 169, 238,
        202, 510, 4, 254, 272, 395, 407, 456,
        9, 410, 97, 277, 208, 152, 336, 60,
        376, 99, 16, 117, 395, 381, 304, 339,
        456, 405, 61, 468, 315, 472, 501, 290,
        127, 456, 504, 132, 225, 32, 275, 199
    }, 1};
}

BitMap BuildInputBits(const BooleanCircuit& circuit,
                      const CiphertextFields& lhs,
                      const CiphertextFields& rhs,
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
                       const CiphertextFields& lhs,
                       const CiphertextFields& rhs,
                       size_t modulus_bits) {
    const auto outputs = GarbleAndEvaluate(
        circuit, BuildInputBits(circuit, lhs, rhs, modulus_bits));
    Require(outputs.size() == 1, "GC must output one predicate bit.");
    return outputs.front();
}

} // namespace

int main() {
    const auto params = ExportedParams();
    const auto circuit = OpenFHELWEIntDecryptCompareCircuit::Describe(params);
    const auto a_prime = ExportedA();
    const auto b_prime = ExportedB();

    Require(EvaluatePredicate(circuit, a_prime, b_prime, params.modulus_bits),
            "GC expected Dec(a') <= Dec(b') for OpenFHE-exported 3 <= 7");
    Require(!EvaluatePredicate(circuit, b_prime, a_prime, params.modulus_bits),
            "GC expected Dec(b') <= Dec(a') to be false for 7 <= 3");
    Require(EvaluatePredicate(circuit, b_prime, b_prime, params.modulus_bits),
            "GC expected Dec(b') <= Dec(b') for 7 <= 7");

#ifdef USE_EMP_GC
    std::cout << "OpenFHE LWE integer decrypt-compare EMP GC test passed.\n";
#else
    std::cout << "OpenFHE LWE integer decrypt-compare minimal GC test passed.\n";
#endif
    return EXIT_SUCCESS;
}

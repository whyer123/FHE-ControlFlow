#include "src/gc/openfhe_lwe_int_decrypt_compare_circuit.h"
#include "src/gc/openfhe_lwe_int_material.h"
#include "src/gc/boolean_circuit_export.h"

#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace {

using BitMap = std::unordered_map<WireId, bool>;

void Require(bool condition, const std::string& message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

bool EvalGate(BitGateKind kind, const std::vector<bool>& inputs) {
    switch (kind) {
    case BitGateKind::And:
        return inputs.at(0) && inputs.at(1);
    case BitGateKind::Xor:
        return inputs.at(0) != inputs.at(1);
    case BitGateKind::Not:
        return !inputs.at(0);
    case BitGateKind::Mux:
        return inputs.at(0) ? inputs.at(1) : inputs.at(2);
    case BitGateKind::Output:
        return inputs.at(0);
    }
    throw std::runtime_error("unknown gate kind");
}

BitMap EvaluatePlain(const BooleanCircuit& circuit, BitMap values) {
    for (const auto& constant : circuit.constant_wires) {
        values[constant.first] = constant.second;
    }
    for (const auto& constant : circuit.secret_constant_wires) {
        values[constant.first] = constant.second;
    }

    for (const auto& gate : circuit.gates) {
        std::vector<bool> inputs;
        inputs.reserve(gate.inputs.size());
        for (const auto wire : gate.inputs) {
            const auto it = values.find(wire);
            Require(it != values.end(), "missing input wire value");
            inputs.push_back(it->second);
        }
        values[gate.output] = EvalGate(gate.kind, inputs);
    }

    return values;
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

bool EvaluateCompare(const BooleanCircuit& circuit,
                     const OpenFHELWEIntCiphertextMaterial& lhs,
                     const OpenFHELWEIntCiphertextMaterial& rhs,
                     size_t modulus_bits) {
    BitMap values;
    SetCiphertext(values, circuit, "lhs", lhs, modulus_bits);
    SetCiphertext(values, circuit, "rhs", rhs, modulus_bits);
    const auto evaluated = EvaluatePlain(circuit, std::move(values));
    Require(circuit.output_wires.size() == 1,
            "decrypt-compare circuit should expose one predicate bit");
    return evaluated.at(circuit.output_wires.at(0));
}

} // namespace

int main(int argc, char** argv) {
    Require(argc == 2, "usage: openfhe_lwe_int_decrypt_compare_circuit_test <material.txt>");

    const auto material = ReadOpenFHELWEIntMaterial(argv[1]);
    const auto params = material.CircuitParams();
    const auto circuit = OpenFHELWEIntDecryptCompareCircuit::Describe(params);
    const auto text = BooleanCircuitToText(circuit, true);

    Require(text.find("[secret_constant_wires]") != std::string::npos,
            "circuit dump must show secret constant section");
    Require(text.find("<selected-label>") != std::string::npos,
            "secret constants must be redacted in circuit dump");
    Require(circuit.secret_constant_wires.size() == params.dimension * 4U,
            "expected duplicated hsk nonzero/sign secret constants per decrypt");

    const bool expected_forward =
        material.a_prime.plaintext <= material.b_prime.plaintext;
    const bool expected_reverse =
        material.b_prime.plaintext <= material.a_prime.plaintext;

    Require(EvaluateCompare(circuit, material.a_prime, material.b_prime,
                            params.modulus_bits) == expected_forward,
            "Dec(a') <= Dec(b') result did not match exported plaintexts");
    Require(EvaluateCompare(circuit, material.b_prime, material.a_prime,
                            params.modulus_bits) == expected_reverse,
            "Dec(b') <= Dec(a') result did not match exported plaintexts");
    Require(EvaluateCompare(circuit, material.b_prime, material.b_prime,
                            params.modulus_bits),
            "expected Dec(b') <= Dec(b') for equal ciphertext inputs");

    std::cout << "OpenFHE LWE integer decrypt-compare circuit test passed.\n";
    return EXIT_SUCCESS;
}

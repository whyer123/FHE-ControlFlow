#include "src/gc/openfhe_lwe_int_decrypt_compare_circuit.h"
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

struct CiphertextFields {
    std::vector<uint64_t> a;
    uint64_t body = 0;
};

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
                   const CiphertextFields& ciphertext,
                   size_t modulus_bits) {
    const auto wires = WireNameMap(circuit);
    for (size_t i = 0; i < ciphertext.a.size(); ++i) {
        SetBits(values, wires, prefix + "_a_" + std::to_string(i),
                ciphertext.a.at(i), modulus_bits);
    }
    SetBits(values, wires, prefix + "_body", ciphertext.body, modulus_bits);
}

bool EvaluateCompare(const BooleanCircuit& circuit,
                     const CiphertextFields& lhs,
                     const CiphertextFields& rhs,
                     size_t modulus_bits) {
    BitMap values;
    SetCiphertext(values, circuit, "lhs", lhs, modulus_bits);
    SetCiphertext(values, circuit, "rhs", rhs, modulus_bits);
    const auto evaluated = EvaluatePlain(circuit, std::move(values));
    Require(circuit.output_wires.size() == 1,
            "decrypt-compare circuit should expose one predicate bit");
    return evaluated.at(circuit.output_wires.at(0));
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

} // namespace

int main() {
    const auto params = ExportedParams();
    const auto circuit = OpenFHELWEIntDecryptCompareCircuit::Describe(params);
    const auto text = BooleanCircuitToText(circuit, true);

    Require(text.find("[secret_constant_wires]") != std::string::npos,
            "circuit dump must show secret constant section");
    Require(text.find("<selected-label>") != std::string::npos,
            "secret constants must be redacted in circuit dump");
    Require(circuit.secret_constant_wires.size() == params.dimension * 4U,
            "expected duplicated hsk nonzero/sign secret constants per decrypt");

    const auto a_prime = ExportedA();
    const auto b_prime = ExportedB();
    Require(EvaluateCompare(circuit, a_prime, b_prime, params.modulus_bits),
            "expected Dec(a') <= Dec(b') for OpenFHE-exported 3 <= 7");
    Require(!EvaluateCompare(circuit, b_prime, a_prime, params.modulus_bits),
            "expected Dec(b') <= Dec(a') to be false for 7 <= 3");
    Require(EvaluateCompare(circuit, b_prime, b_prime, params.modulus_bits),
            "expected Dec(b') <= Dec(b') for 7 <= 7");

    std::cout << "OpenFHE LWE integer decrypt-compare circuit test passed.\n";
    return EXIT_SUCCESS;
}

#include "src/gc/openfhe_eval_bin_gate_circuit.h"
#include "src/gc/boolean_circuit_export.h"
#include "src/gc/openfhe_lwe_decryption_circuit.h"

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

uint64_t BitsToInteger(const std::vector<bool>& bits) {
    uint64_t value = 0;
    for (size_t i = 0; i < bits.size(); ++i) {
        if (bits[i]) {
            value |= (1ULL << i);
        }
    }
    return value;
}

std::vector<bool> IntegerToBits(uint64_t value) {
    std::vector<bool> bits;
    bits.reserve(OpenFHELWEDecryptionCircuit::kDemoModulusBits);
    for (size_t i = 0; i < OpenFHELWEDecryptionCircuit::kDemoModulusBits; ++i) {
        bits.push_back(((value >> i) & 1U) != 0);
    }
    return bits;
}

uint64_t DemoPad() {
    const auto& hsk = OpenFHELWEDecryptionCircuit::FixedDemoSecretKey();
    const auto& mask = OpenFHELWEDecryptionCircuit::FixedDemoMask();
    uint64_t pad = 0;
    for (size_t i = 0; i < mask.size(); ++i) {
        if (hsk[i]) {
            pad = (pad + mask[i]) % OpenFHELWEDecryptionCircuit::kDemoModulus;
        }
    }
    return pad;
}

std::vector<bool> DemoCiphertextBody(bool message) {
    const uint64_t encoded =
        message ? OpenFHELWEDecryptionCircuit::kDemoPlaintextScale : 0;
    return IntegerToBits((DemoPad() + encoded) %
                         OpenFHELWEDecryptionCircuit::kDemoModulus);
}

void SetCiphertextInput(const BooleanCircuit& circuit, BitMap& values,
                        const std::string& prefix, bool message) {
    const auto& mask = OpenFHELWEDecryptionCircuit::FixedDemoMask();
    const auto body = DemoCiphertextBody(message);

    for (const auto wire : circuit.input_wires) {
        const auto& name = circuit.Wire(wire).name;
        for (size_t i = 0; i < mask.size(); ++i) {
            const auto mask_prefix =
                prefix + "_a_" + std::to_string(i) + "_bit_";
            if (name.rfind(mask_prefix, 0) == 0) {
                const auto bit = std::stoul(name.substr(mask_prefix.size()));
                values[wire] = ((mask[i] >> bit) & 1U) != 0;
            }
        }

        const auto body_prefix = prefix + "_b_bit_";
        if (name.rfind(body_prefix, 0) == 0) {
            const auto bit = std::stoul(name.substr(body_prefix.size()));
            values[wire] = body.at(bit);
        }
    }
}

bool ReadOutputCiphertextBit(const BooleanCircuit& circuit, const BitMap& values) {
    std::vector<bool> mask_bits_flat;
    std::vector<uint64_t> mask(OpenFHELWEDecryptionCircuit::kDemoDimension, 0);
    std::vector<bool> body_bits(OpenFHELWEDecryptionCircuit::kDemoModulusBits);

    for (const auto wire : circuit.output_wires) {
        const auto value = values.at(wire);
        const auto& name = circuit.Wire(wire).name;
        for (size_t i = 0; i < mask.size(); ++i) {
            const auto mask_prefix =
                "evalbingate_out_a_" + std::to_string(i) + "_bit_";
            if (name.rfind(mask_prefix, 0) == 0) {
                const auto bit = std::stoul(name.substr(mask_prefix.size()));
                if (value) {
                    mask[i] |= (1ULL << bit);
                }
            }
        }

        const std::string body_prefix = "evalbingate_out_b_bit_";
        if (name.rfind(body_prefix, 0) == 0) {
            const auto bit = std::stoul(name.substr(body_prefix.size()));
            body_bits.at(bit) = value;
        }
    }

    uint64_t pad = 0;
    const auto& hsk = OpenFHELWEDecryptionCircuit::FixedDemoSecretKey();
    for (size_t i = 0; i < mask.size(); ++i) {
        if (hsk[i]) {
            pad = (pad + mask[i]) % OpenFHELWEDecryptionCircuit::kDemoModulus;
        }
    }

    const auto body = BitsToInteger(body_bits);
    const auto phase =
        (body + OpenFHELWEDecryptionCircuit::kDemoModulus - pad) %
        OpenFHELWEDecryptionCircuit::kDemoModulus;
    const auto rounded =
        (phase + OpenFHELWEDecryptionCircuit::kDemoModulus /
                     (OpenFHELWEDecryptionCircuit::kDemoPlaintextModulus * 2)) %
        OpenFHELWEDecryptionCircuit::kDemoModulus;
    return ((rounded >> 2) & 1U) != 0;
}

bool Expected(OpenFHEBinGateKind gate, bool lhs, bool rhs) {
    switch (gate) {
    case OpenFHEBinGateKind::And:
        return lhs && rhs;
    case OpenFHEBinGateKind::Or:
        return lhs || rhs;
    case OpenFHEBinGateKind::Xor:
        return lhs != rhs;
    case OpenFHEBinGateKind::Xnor:
        return lhs == rhs;
    }
    throw std::runtime_error("unknown OpenFHE gate kind");
}

void CheckGate(OpenFHEBinGateKind gate) {
    const auto circuit =
        OpenFHEEvalBinGateCircuit::DescribeDemoEvalBinGate(gate);
    const auto text = BooleanCircuitToText(circuit);

    Require(text.find("openfhe_evalbingate_prebootstrap") != std::string::npos,
            "EvalBinGate circuit must expose the OpenFHE pre-bootstrap stage");
    Require(text.find("openfhe_bootstrap_placeholder") != std::string::npos,
            "EvalBinGate circuit must explicitly mark the bootstrap placeholder");
    Require(circuit.output_wires.size() ==
                OpenFHELWEDecryptionCircuit::kDemoDimension *
                    OpenFHELWEDecryptionCircuit::kDemoModulusBits +
                    OpenFHELWEDecryptionCircuit::kDemoModulusBits,
            "EvalBinGate circuit must output one LWE ciphertext");

    for (const bool lhs : {false, true}) {
        for (const bool rhs : {false, true}) {
            BitMap values;
            SetCiphertextInput(circuit, values, "lhs", lhs);
            SetCiphertextInput(circuit, values, "rhs", rhs);
            const auto evaluated = EvaluatePlain(circuit, std::move(values));
            const bool actual = ReadOutputCiphertextBit(circuit, evaluated);
            Require(actual == Expected(gate, lhs, rhs),
                    "EvalBinGate circuit produced the wrong gate result");
        }
    }
}

} // namespace

int main() {
    CheckGate(OpenFHEBinGateKind::And);
    CheckGate(OpenFHEBinGateKind::Or);
    CheckGate(OpenFHEBinGateKind::Xor);
    CheckGate(OpenFHEBinGateKind::Xnor);

    std::cout << "OpenFHE EvalBinGate Boolean circuit expansion test passed.\n";
    return EXIT_SUCCESS;
}

#include "minimal_garbled_circuit.h"
#include <algorithm>
#include <iomanip>
#include <random>
#include <sstream>
#include <stdexcept>

namespace {

uint64_t StableHash64(const std::string& input) {
    uint64_t hash = 1469598103934665603ULL;
    for (const auto ch : input) {
        hash ^= static_cast<unsigned char>(ch);
        hash *= 1099511628211ULL;
    }
    hash ^= hash >> 33;
    hash *= 0xff51afd7ed558ccdULL;
    hash ^= hash >> 33;
    hash *= 0xc4ceb9fe1a85ec53ULL;
    hash ^= hash >> 33;
    return hash;
}

std::string Hex64(uint64_t value) {
    std::ostringstream out;
    out << std::hex << std::setw(16) << std::setfill('0') << value;
    return out.str();
}

std::string XorStrings(const std::string& value, const std::string& mask) {
    if (value.size() != mask.size()) {
        throw std::invalid_argument("XOR string inputs must have equal length.");
    }

    std::string result(value.size(), '\0');
    for (size_t i = 0; i < value.size(); ++i) {
        result[i] = static_cast<char>(value[i] ^ mask[i]);
    }
    return result;
}

} // namespace

GarbledCircuitArtifact MinimalGarbledCircuit::Garble(
    const BooleanCircuit& circuit) const {
    GarbledCircuitArtifact artifact;
    artifact.name = circuit.name;
    artifact.input_wires = circuit.input_wires;
    artifact.output_wires = circuit.output_wires;

    std::mt19937_64 rng(StableHash64(circuit.name + "|" +
                                      std::to_string(circuit.gates.size())));
    std::unordered_map<WireId, WireLabels> wire_labels;

    for (const auto& wire : circuit.wires) {
        wire_labels.emplace(
            wire.id,
            WireLabels{{MakeLabel(wire.id, false, rng()),
                        MakeLabel(wire.id, true, rng())}});
    }

    for (const auto wire : circuit.input_wires) {
        artifact.public_input_labels.emplace(wire, wire_labels.at(wire));
    }

    for (const auto& constant : circuit.constant_wires) {
        artifact.constant_labels.emplace(
            constant.first,
            wire_labels.at(constant.first).labels[constant.second ? 1 : 0]);
    }

    for (const auto wire : circuit.output_wires) {
        const auto& labels = wire_labels.at(wire);
        artifact.output_decoding[wire].emplace(labels.labels[0], false);
        artifact.output_decoding[wire].emplace(labels.labels[1], true);
    }

    for (const auto& gate : circuit.gates) {
        artifact.gates.push_back(
            {gate.id, gate.kind, gate.inputs, gate.output, GarbleGate(gate, wire_labels)});
    }

    return artifact;
}

std::vector<bool> MinimalGarbledCircuit::Evaluate(
    const GarbledCircuitArtifact& artifact,
    const std::unordered_map<WireId, bool>& input_bits) const {
    return DecodeOutputs(artifact, EvaluateLabels(artifact, EncodeInputs(artifact, input_bits)));
}

std::unordered_map<WireId, std::string> MinimalGarbledCircuit::EncodeInputs(
    const GarbledCircuitArtifact& artifact,
    const std::unordered_map<WireId, bool>& input_bits) const {
    std::unordered_map<WireId, std::string> input_labels;

    for (const auto wire : artifact.input_wires) {
        const auto input_it = input_bits.find(wire);
        if (input_it == input_bits.end()) {
            throw std::invalid_argument("Missing garbled circuit input bit.");
        }
        const auto labels_it = artifact.public_input_labels.find(wire);
        if (labels_it == artifact.public_input_labels.end()) {
            throw std::invalid_argument("Missing input wire labels.");
        }
        input_labels.emplace(wire, labels_it->second.labels[input_it->second ? 1 : 0]);
    }

    return input_labels;
}

std::vector<std::string> MinimalGarbledCircuit::EvaluateLabels(
    const GarbledCircuitArtifact& artifact,
    const std::unordered_map<WireId, std::string>& input_labels) const {
    std::unordered_map<WireId, std::string> wire_labels;

    for (const auto wire : artifact.input_wires) {
        const auto input_it = input_labels.find(wire);
        if (input_it == input_labels.end()) {
            throw std::invalid_argument("Missing garbled circuit input label.");
        }
        wire_labels.emplace(wire, input_it->second);
    }

    for (const auto& constant : artifact.constant_labels) {
        wire_labels.emplace(constant.first, constant.second);
    }

    for (const auto& gate : artifact.gates) {
        std::vector<std::string> input_labels_for_gate;
        input_labels_for_gate.reserve(gate.inputs.size());
        for (const auto wire : gate.inputs) {
            const auto label_it = wire_labels.find(wire);
            if (label_it == wire_labels.end()) {
                throw std::invalid_argument("Gate references an unset wire label.");
            }
            input_labels_for_gate.push_back(label_it->second);
        }

        bool found = false;
        for (const auto& entry : gate.table) {
            const auto pad = DerivePad(gate.id, input_labels_for_gate,
                                       entry.encrypted_label.size());
            const auto candidate = XorStrings(entry.encrypted_label, pad);
            if (LabelTag(gate.id, candidate) == entry.tag) {
                wire_labels[gate.output] = candidate;
                found = true;
                break;
            }
        }

        if (!found) {
            throw std::invalid_argument("No garbled table row matched the input labels.");
        }
    }

    std::vector<std::string> outputs;
    outputs.reserve(artifact.output_wires.size());
    for (const auto wire : artifact.output_wires) {
        const auto label_it = wire_labels.find(wire);
        if (label_it == wire_labels.end()) {
            throw std::invalid_argument("Missing garbled circuit output label.");
        }
        outputs.push_back(label_it->second);
    }

    return outputs;
}

std::vector<bool> MinimalGarbledCircuit::DecodeOutputs(
    const GarbledCircuitArtifact& artifact,
    const std::vector<std::string>& output_labels) const {
    if (output_labels.size() != artifact.output_wires.size()) {
        throw std::invalid_argument("Output label count does not match circuit outputs.");
    }

    std::vector<bool> outputs;
    outputs.reserve(output_labels.size());
    for (size_t i = 0; i < output_labels.size(); ++i) {
        outputs.push_back(
            DecodeOutputLabel(artifact, artifact.output_wires[i], output_labels[i]));
    }
    return outputs;
}

bool MinimalGarbledCircuit::EvalGate(BitGateKind kind,
                                     const std::vector<bool>& inputs) const {
    switch (kind) {
    case BitGateKind::And:
        if (inputs.size() != 2) {
            throw std::invalid_argument("AND expects two inputs.");
        }
        return inputs[0] && inputs[1];
    case BitGateKind::Xor:
        if (inputs.size() != 2) {
            throw std::invalid_argument("XOR expects two inputs.");
        }
        return inputs[0] != inputs[1];
    case BitGateKind::Not:
        if (inputs.size() != 1) {
            throw std::invalid_argument("NOT expects one input.");
        }
        return !inputs[0];
    case BitGateKind::Mux:
        if (inputs.size() != 3) {
            throw std::invalid_argument("MUX expects three inputs.");
        }
        return inputs[0] ? inputs[1] : inputs[2];
    case BitGateKind::Output:
        if (inputs.size() != 1) {
            throw std::invalid_argument("OUTPUT expects one input.");
        }
        return inputs[0];
    }

    throw std::invalid_argument("Unsupported gate kind.");
}

std::vector<GarbledTableEntry> MinimalGarbledCircuit::GarbleGate(
    const CircuitGate& gate,
    const std::unordered_map<WireId, WireLabels>& wire_labels) const {
    std::vector<GarbledTableEntry> table;
    const size_t row_count = 1ULL << gate.inputs.size();

    for (size_t row = 0; row < row_count; ++row) {
        std::vector<bool> input_bits;
        std::vector<std::string> input_labels;
        input_bits.reserve(gate.inputs.size());
        input_labels.reserve(gate.inputs.size());

        for (size_t i = 0; i < gate.inputs.size(); ++i) {
            const bool bit = ((row >> i) & 1U) != 0;
            const auto labels_it = wire_labels.find(gate.inputs[i]);
            if (labels_it == wire_labels.end()) {
                throw std::invalid_argument("Gate input is missing labels.");
            }
            input_bits.push_back(bit);
            input_labels.push_back(labels_it->second.labels[bit ? 1 : 0]);
        }

        const bool output_bit = EvalGate(gate.kind, input_bits);
        const auto output_labels_it = wire_labels.find(gate.output);
        if (output_labels_it == wire_labels.end()) {
            throw std::invalid_argument("Gate output is missing labels.");
        }

        const auto& output_label = output_labels_it->second.labels[output_bit ? 1 : 0];
        const auto pad = DerivePad(gate.id, input_labels, output_label.size());
        table.push_back({XorStrings(output_label, pad),
                         LabelTag(gate.id, output_label)});
    }

    std::reverse(table.begin(), table.end());
    return table;
}

bool MinimalGarbledCircuit::DecodeOutputLabel(
    const GarbledCircuitArtifact& artifact, WireId wire,
    const std::string& label) const {
    const auto output_it = artifact.output_decoding.find(wire);
    if (output_it == artifact.output_decoding.end()) {
        throw std::invalid_argument("Missing output decoding table.");
    }

    const auto value_it = output_it->second.find(label);
    if (value_it == output_it->second.end()) {
        throw std::invalid_argument("Output label is not decodable.");
    }

    return value_it->second;
}

std::string MinimalGarbledCircuit::DerivePad(
    GateId gate, const std::vector<std::string>& input_labels,
    size_t length) const {
    std::string seed = "pad|" + std::to_string(gate);
    for (const auto& label : input_labels) {
        seed += "|" + label;
    }

    std::string pad;
    uint64_t counter = 0;
    while (pad.size() < length) {
        pad += Hex64(StableHash64(seed + "|" + std::to_string(counter++)));
    }
    pad.resize(length);
    return pad;
}

std::string MinimalGarbledCircuit::LabelTag(GateId gate,
                                            const std::string& label) const {
    return Hex64(StableHash64("tag|" + std::to_string(gate) + "|" + label));
}

std::string MinimalGarbledCircuit::MakeLabel(WireId wire, bool bit,
                                             uint64_t nonce) const {
    const auto domain = "label|" + std::to_string(wire) + "|" +
                        std::to_string(bit ? 1 : 0) + "|" +
                        std::to_string(nonce);
    return Hex64(StableHash64(domain + "|0")) +
           Hex64(StableHash64(domain + "|1")) +
           Hex64(StableHash64(domain + "|2")) +
           Hex64(StableHash64(domain + "|3"));
}

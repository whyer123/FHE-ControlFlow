#include "minimal_garbled_circuit.h"
#include <sstream>
#include <stdexcept>

GarbledCircuitArtifact MinimalGarbledCircuit::Garble(
    const BooleanCircuit& circuit) const {
    GarbledCircuitArtifact artifact;
    artifact.name = circuit.name;
    artifact.input_wires = circuit.input_wires;
    artifact.output_wires = circuit.output_wires;

    uint64_t nonce = 0xC0DEC0DE12345678ULL;
    for (const auto& wire : circuit.wires) {
        artifact.all_wire_labels.emplace(
            wire.id,
            WireLabels{{MakeLabel(wire.id, false, nonce),
                        MakeLabel(wire.id, true,
                                  nonce ^ 0x9E3779B97F4A7C15ULL)}});
        nonce += 0xD1B54A32D192ED03ULL;
    }

    for (const auto& gate : circuit.gates) {
        artifact.gates.push_back({gate.id, gate.kind, gate.inputs, gate.output});
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
        const auto labels_it = artifact.all_wire_labels.find(wire);
        if (labels_it == artifact.all_wire_labels.end()) {
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
        DecodeWireLabel(artifact, wire, input_it->second);
        wire_labels.emplace(wire, input_it->second);
    }

    for (const auto& gate : artifact.gates) {
        std::vector<bool> inputs;
        inputs.reserve(gate.inputs.size());
        for (const auto wire : gate.inputs) {
            const auto label_it = wire_labels.find(wire);
            if (label_it == wire_labels.end()) {
                throw std::invalid_argument("Gate references an unset wire label.");
            }
            inputs.push_back(DecodeWireLabel(artifact, wire, label_it->second));
        }

        const auto output_labels_it = artifact.all_wire_labels.find(gate.output);
        if (output_labels_it == artifact.all_wire_labels.end()) {
            throw std::invalid_argument("Missing output wire labels.");
        }
        const bool output_bit = EvalGate(gate.kind, inputs);
        wire_labels[gate.output] = output_labels_it->second.labels[output_bit ? 1 : 0];
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
            DecodeWireLabel(artifact, artifact.output_wires[i], output_labels[i]));
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

bool MinimalGarbledCircuit::DecodeWireLabel(const GarbledCircuitArtifact& artifact,
                                            WireId wire,
                                            const std::string& label) const {
    const auto labels_it = artifact.all_wire_labels.find(wire);
    if (labels_it == artifact.all_wire_labels.end()) {
        throw std::invalid_argument("Missing wire labels.");
    }

    if (label == labels_it->second.labels[0]) {
        return false;
    }
    if (label == labels_it->second.labels[1]) {
        return true;
    }

    throw std::invalid_argument("Label does not match the requested wire.");
}

std::string MinimalGarbledCircuit::MakeLabel(WireId wire, bool bit,
                                             uint64_t nonce) const {
    std::ostringstream out;
    out << "L" << wire << "_" << (bit ? 1 : 0) << "_" << std::hex << nonce;
    return out.str();
}

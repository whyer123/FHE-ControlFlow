#pragma once

#include "src/gc/boolean_circuit.h"
#include <array>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

struct WireLabels {
    std::array<std::string, 2> labels;
};

struct GarbledTableEntry {
    std::string encrypted_label;
    std::string tag;
};

struct GarbledGate {
    GateId id = 0;
    BitGateKind kind;
    std::vector<WireId> inputs;
    WireId output = 0;
    std::vector<GarbledTableEntry> table;
};

struct GarbledCircuitArtifact {
    std::string name;
    std::vector<WireId> input_wires;
    std::vector<WireId> output_wires;
    std::unordered_map<WireId, std::string> constant_labels;
    std::vector<GarbledGate> gates;
    std::unordered_map<WireId, WireLabels> public_input_labels;
    std::unordered_map<WireId, std::unordered_map<std::string, bool>> output_decoding;
};

class MinimalGarbledCircuit {
public:
    GarbledCircuitArtifact Garble(const BooleanCircuit& circuit) const;

    std::vector<bool> Evaluate(
        const GarbledCircuitArtifact& artifact,
        const std::unordered_map<WireId, bool>& input_bits) const;

    std::unordered_map<WireId, std::string> EncodeInputs(
        const GarbledCircuitArtifact& artifact,
        const std::unordered_map<WireId, bool>& input_bits) const;

    std::vector<std::string> EvaluateLabels(
        const GarbledCircuitArtifact& artifact,
        const std::unordered_map<WireId, std::string>& input_labels) const;

    std::vector<bool> DecodeOutputs(
        const GarbledCircuitArtifact& artifact,
        const std::vector<std::string>& output_labels) const;

private:
    bool EvalGate(BitGateKind kind, const std::vector<bool>& inputs) const;
    std::vector<GarbledTableEntry> GarbleGate(
        const CircuitGate& gate,
        const std::unordered_map<WireId, WireLabels>& wire_labels) const;
    bool DecodeOutputLabel(const GarbledCircuitArtifact& artifact, WireId wire,
                           const std::string& label) const;
    std::string DerivePad(GateId gate, const std::vector<std::string>& input_labels,
                          size_t length) const;
    std::string LabelTag(GateId gate, const std::string& label) const;
    std::string MakeLabel(WireId wire, bool bit, uint64_t nonce) const;
};

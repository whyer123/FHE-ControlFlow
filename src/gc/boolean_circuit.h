#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

using WireId = uint32_t;
using GateId = uint32_t;

enum class BitGateKind {
    And,
    Xor,
    Not,
    Mux,
    Output
};

struct CircuitWire {
    WireId id = 0;
    std::string name;
};

struct CircuitGate {
    GateId id = 0;
    BitGateKind kind;
    std::vector<WireId> inputs;
    WireId output = 0;
};

struct BooleanCircuit {
    std::string name;
    size_t input_bit_length = 0;
    std::vector<WireId> input_wires;
    std::vector<WireId> output_wires;
    std::vector<CircuitWire> wires;
    std::vector<CircuitGate> gates;

    const CircuitWire& Wire(WireId id) const;
};

class BooleanCircuitBuilder {
public:
    BooleanCircuitBuilder(std::string name, size_t input_bit_length);

    WireId AddInputWire(const std::string& name);
    WireId AddGate(BitGateKind kind, const std::vector<WireId>& inputs,
                   const std::string& output_name);
    void AddOutputWire(WireId wire);

    BooleanCircuit Build() const;

private:
    BooleanCircuit circuit_;
    std::unordered_map<WireId, size_t> wire_index_;
    WireId next_wire_id_ = 0;
    GateId next_gate_id_ = 0;
};

std::string BitGateKindToString(BitGateKind kind);

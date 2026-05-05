#include "boolean_circuit.h"
#include <stdexcept>
#include <utility>

const CircuitWire& BooleanCircuit::Wire(WireId id) const {
    for (const auto& wire : wires) {
        if (wire.id == id) {
            return wire;
        }
    }
    throw std::out_of_range("Unknown circuit wire id.");
}

BooleanCircuitBuilder::BooleanCircuitBuilder(std::string name,
                                             size_t input_bit_length) {
    circuit_.name = std::move(name);
    circuit_.input_bit_length = input_bit_length;
}

WireId BooleanCircuitBuilder::AddInputWire(const std::string& name) {
    const WireId id = next_wire_id_++;
    circuit_.wires.push_back({id, name});
    circuit_.input_wires.push_back(id);
    wire_index_[id] = circuit_.wires.size() - 1;
    return id;
}

WireId BooleanCircuitBuilder::AddConstantWire(const std::string& name, bool value) {
    const WireId id = next_wire_id_++;
    circuit_.wires.push_back({id, name});
    circuit_.constant_wires.emplace(id, value);
    wire_index_[id] = circuit_.wires.size() - 1;
    return id;
}

WireId BooleanCircuitBuilder::AddGate(BitGateKind kind,
                                      const std::vector<WireId>& inputs,
                                      const std::string& output_name) {
    for (const auto input : inputs) {
        if (wire_index_.find(input) == wire_index_.end()) {
            throw std::invalid_argument("Gate references an unknown input wire.");
        }
    }

    const WireId output = next_wire_id_++;
    circuit_.wires.push_back({output, output_name});
    wire_index_[output] = circuit_.wires.size() - 1;
    circuit_.gates.push_back({next_gate_id_++, kind, inputs, output});
    return output;
}

void BooleanCircuitBuilder::AddOutputWire(WireId wire) {
    if (wire_index_.find(wire) == wire_index_.end()) {
        throw std::invalid_argument("Output references an unknown wire.");
    }
    circuit_.output_wires.push_back(wire);
}

BooleanCircuit BooleanCircuitBuilder::Build() const {
    return circuit_;
}

std::string BitGateKindToString(BitGateKind kind) {
    switch (kind) {
    case BitGateKind::And:
        return "AND";
    case BitGateKind::Xor:
        return "XOR";
    case BitGateKind::Not:
        return "NOT";
    case BitGateKind::Mux:
        return "MUX";
    case BitGateKind::Output:
        return "OUTPUT";
    }
    return "UNKNOWN";
}

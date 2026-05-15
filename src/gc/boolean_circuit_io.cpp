#include "boolean_circuit_io.h"

#include <algorithm>
#include <cstdint>
#include <fstream>
#include <stdexcept>

namespace {

constexpr const char* kMagic = "FHE_CIRCUIT_SHAPE_V1";

void Require(bool condition, const std::string& message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

void WriteU64(std::ostream& out, uint64_t value) {
    out.write(reinterpret_cast<const char*>(&value), sizeof(value));
}

uint64_t ReadU64(std::istream& in) {
    uint64_t value = 0;
    in.read(reinterpret_cast<char*>(&value), sizeof(value));
    Require(static_cast<bool>(in), "failed to read uint64 from circuit shape.");
    return value;
}

void WriteString(std::ostream& out, const std::string& value) {
    WriteU64(out, value.size());
    out.write(value.data(), static_cast<std::streamsize>(value.size()));
}

std::string ReadString(std::istream& in) {
    const auto size = ReadU64(in);
    std::string value(size, '\0');
    in.read(value.data(), static_cast<std::streamsize>(size));
    Require(static_cast<bool>(in), "failed to read string from circuit shape.");
    return value;
}

void WriteWireVector(std::ostream& out, const std::vector<WireId>& wires) {
    WriteU64(out, wires.size());
    for (const auto wire : wires) {
        WriteU64(out, wire);
    }
}

std::vector<WireId> ReadWireVector(std::istream& in) {
    const auto size = ReadU64(in);
    std::vector<WireId> wires;
    wires.reserve(size);
    for (uint64_t i = 0; i < size; ++i) {
        wires.push_back(static_cast<WireId>(ReadU64(in)));
    }
    return wires;
}

} // namespace

void WriteBooleanCircuitShape(const BooleanCircuit& circuit,
                              const std::string& path) {
    std::ofstream out(path, std::ios::binary);
    Require(out.good(), "failed to open circuit shape for writing: " + path);

    WriteString(out, kMagic);
    WriteString(out, circuit.name);
    WriteU64(out, circuit.input_bit_length);
    WriteWireVector(out, circuit.input_wires);
    WriteWireVector(out, circuit.output_wires);

    std::vector<WireId> constant_wires;
    constant_wires.reserve(circuit.constant_wires.size());
    for (const auto& constant : circuit.constant_wires) {
        constant_wires.push_back(constant.first);
    }
    std::sort(constant_wires.begin(), constant_wires.end());
    WriteWireVector(out, constant_wires);

    WriteU64(out, circuit.wires.size());
    for (const auto& wire : circuit.wires) {
        WriteU64(out, wire.id);
        WriteString(out, wire.name);
    }

    WriteU64(out, circuit.gates.size());
    for (const auto& gate : circuit.gates) {
        WriteU64(out, gate.id);
        WriteU64(out, static_cast<uint64_t>(gate.kind));
        WriteWireVector(out, gate.inputs);
        WriteU64(out, gate.output);
    }

    Require(static_cast<bool>(out), "failed to write circuit shape: " + path);
}

BooleanCircuit ReadBooleanCircuitShape(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    Require(in.good(), "failed to open circuit shape for reading: " + path);

    const auto magic = ReadString(in);
    Require(magic == kMagic, "invalid circuit shape magic: " + path);

    BooleanCircuit circuit;
    circuit.name = ReadString(in);
    circuit.input_bit_length = static_cast<size_t>(ReadU64(in));
    circuit.input_wires = ReadWireVector(in);
    circuit.output_wires = ReadWireVector(in);

    for (const auto wire : ReadWireVector(in)) {
        circuit.constant_wires.emplace(wire, false);
    }

    const auto wire_count = ReadU64(in);
    circuit.wires.reserve(wire_count);
    for (uint64_t i = 0; i < wire_count; ++i) {
        CircuitWire wire;
        wire.id = static_cast<WireId>(ReadU64(in));
        wire.name = ReadString(in);
        circuit.wires.push_back(std::move(wire));
    }

    const auto gate_count = ReadU64(in);
    circuit.gates.reserve(gate_count);
    for (uint64_t i = 0; i < gate_count; ++i) {
        CircuitGate gate;
        gate.id = static_cast<GateId>(ReadU64(in));
        gate.kind = static_cast<BitGateKind>(ReadU64(in));
        gate.inputs = ReadWireVector(in);
        gate.output = static_cast<WireId>(ReadU64(in));
        circuit.gates.push_back(std::move(gate));
    }

    return circuit;
}

#include "boolean_circuit_export.h"
#include <algorithm>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace {

std::vector<std::pair<WireId, bool>> SortedConstants(
    const BooleanCircuit& circuit) {
    std::vector<std::pair<WireId, bool>> constants(
        circuit.constant_wires.begin(), circuit.constant_wires.end());
    std::sort(constants.begin(), constants.end(),
              [](const auto& lhs, const auto& rhs) {
                  return lhs.first < rhs.first;
              });
    return constants;
}

void WriteWireList(std::ostringstream& out, const BooleanCircuit& circuit,
                   const std::vector<WireId>& wires) {
    for (const auto wire : wires) {
        out << "  w" << wire << " " << circuit.Wire(wire).name << "\n";
    }
}

} // namespace

std::string BooleanCircuitToText(const BooleanCircuit& circuit,
                                 bool redact_constant_values) {
    std::ostringstream out;
    out << "# Boolean circuit dump\n";
    out << "name: " << circuit.name << "\n";
    out << "input_bit_length: " << circuit.input_bit_length << "\n";
    out << "wire_count: " << circuit.wires.size() << "\n";
    out << "gate_count: " << circuit.gates.size() << "\n\n";

    out << "[public_inputs]\n";
    WriteWireList(out, circuit, circuit.input_wires);
    out << "\n";

    out << "[constant_wires]\n";
    for (const auto& constant : SortedConstants(circuit)) {
        out << "  w" << constant.first << " "
            << circuit.Wire(constant.first).name << " = ";
        if (redact_constant_values) {
            out << "<selected-label>";
        } else {
            out << (constant.second ? 1 : 0);
        }
        out << "\n";
    }
    out << "\n";

    out << "[outputs]\n";
    WriteWireList(out, circuit, circuit.output_wires);
    out << "\n";

    out << "[gates]\n";
    for (const auto& gate : circuit.gates) {
        out << "  g" << gate.id << ": w" << gate.output
            << "(" << circuit.Wire(gate.output).name << ") <- "
            << BitGateKindToString(gate.kind) << "(";
        for (size_t i = 0; i < gate.inputs.size(); ++i) {
            if (i > 0) {
                out << ", ";
            }
            out << "w" << gate.inputs[i]
                << "(" << circuit.Wire(gate.inputs[i]).name << ")";
        }
        out << ")\n";
    }

    return out.str();
}

void WriteBooleanCircuitText(const BooleanCircuit& circuit,
                             const std::string& path,
                             bool redact_constant_values) {
    std::ofstream file(path);
    if (!file) {
        throw std::runtime_error("Failed to open Boolean circuit dump path.");
    }
    file << BooleanCircuitToText(circuit, redact_constant_values);
}

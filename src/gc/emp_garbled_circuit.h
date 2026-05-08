#pragma once

#ifdef USE_EMP_GC

#include "src/gc/boolean_circuit.h"
#include <array>
#include <emp-tool/emp-tool.h>
#include <string>
#include <unordered_map>
#include <vector>

struct EmpWireLabels {
    std::array<emp::block, 2> labels;
};

struct EmpGarbledCircuitArtifact {
    std::string name;
    size_t input_bit_length = 0;
    std::vector<WireId> input_wires;
    std::vector<WireId> output_wires;
    std::unordered_map<WireId, EmpWireLabels> public_input_labels;
    std::unordered_map<WireId, emp::block> constant_labels;
    std::unordered_map<WireId, emp::block> output_zero_labels;
    std::vector<emp::block> transcript;
    emp::block delta;
    size_t gate_count = 0;
    size_t and_gate_count = 0;
};

class EmpGarbledCircuit {
public:
    EmpGarbledCircuitArtifact Garble(const BooleanCircuit& circuit) const;

    std::vector<bool> Evaluate(
        const EmpGarbledCircuitArtifact& artifact,
        const BooleanCircuit& circuit,
        const std::unordered_map<WireId, bool>& input_bits) const;
};

#endif

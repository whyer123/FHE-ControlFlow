#include "emp_garbled_circuit.h"

#ifdef USE_EMP_GC

#include <cstring>
#include <emp-tool/gc/halfgate_eva.h>
#include <emp-tool/gc/halfgate_gen.h>
#include <stdexcept>

namespace {

class EmpTranscriptIO {
public:
    explicit EmpTranscriptIO(std::vector<emp::block>* output)
        : output_(output), input_(nullptr) {}

    explicit EmpTranscriptIO(const std::vector<emp::block>* input)
        : output_(nullptr), input_(input) {}

    void send_block(const emp::block* data, int count) {
        if (output_ == nullptr || count < 0) {
            throw std::runtime_error("EMP transcript is not writable.");
        }
        output_->insert(output_->end(), data, data + count);
    }

    void recv_block(emp::block* data, int count) {
        if (input_ == nullptr || count < 0 ||
            read_cursor_ + static_cast<size_t>(count) > input_->size()) {
            throw std::runtime_error("EMP transcript underflow.");
        }
        std::memcpy(data, input_->data() + read_cursor_,
                    sizeof(emp::block) * static_cast<size_t>(count));
        read_cursor_ += static_cast<size_t>(count);
    }

private:
    std::vector<emp::block>* output_;
    const std::vector<emp::block>* input_;
    size_t read_cursor_ = 0;
};

emp::block RandomBlock() {
    emp::block label;
    emp::PRG().random_block(&label, 1);
    return label;
}

bool BlockEquals(const emp::block& lhs, const emp::block& rhs) {
    return std::memcmp(&lhs, &rhs, sizeof(emp::block)) == 0;
}

emp::block LabelOne(const emp::block& label_zero, const emp::block& delta) {
    return label_zero ^ delta;
}

emp::block RequireWireLabel(
    const std::unordered_map<WireId, emp::block>& wire_labels,
    WireId wire) {
    const auto it = wire_labels.find(wire);
    if (it == wire_labels.end()) {
        throw std::invalid_argument("EMP GC gate references an unset wire.");
    }
    return it->second;
}

} // namespace

EmpGarbledCircuitArtifact EmpGarbledCircuit::Garble(
    const BooleanCircuit& circuit) const {
    EmpGarbledCircuitArtifact artifact;
    artifact.name = circuit.name;
    artifact.input_bit_length = circuit.input_bit_length;
    artifact.input_wires = circuit.input_wires;
    artifact.output_wires = circuit.output_wires;
    artifact.gate_count = circuit.gates.size();

    EmpTranscriptIO gen_io(&artifact.transcript);
    emp::HalfGateGen<EmpTranscriptIO> gen(&gen_io);
    artifact.delta = gen.delta;

    std::unordered_map<WireId, emp::block> zero_labels;

    for (const auto wire : circuit.input_wires) {
        auto zero = RandomBlock();
        zero_labels.emplace(wire, zero);
        artifact.public_input_labels.emplace(
            wire, EmpWireLabels{{zero, LabelOne(zero, artifact.delta)}});
    }

    for (const auto& constant : circuit.constant_wires) {
        auto zero = RandomBlock();
        zero_labels.emplace(constant.first, zero);
        artifact.constant_labels.emplace(
            constant.first,
            constant.second ? LabelOne(zero, artifact.delta) : zero);
    }

    for (const auto& gate : circuit.gates) {
        emp::block output_zero;
        switch (gate.kind) {
        case BitGateKind::And: {
            if (gate.inputs.size() != 2) {
                throw std::invalid_argument("EMP AND gate expects two inputs.");
            }
            auto lhs_zero = RequireWireLabel(zero_labels, gate.inputs[0]);
            auto rhs_zero = RequireWireLabel(zero_labels, gate.inputs[1]);
            output_zero = gen.and_gate(lhs_zero, rhs_zero);
            ++artifact.and_gate_count;
            break;
        }
        case BitGateKind::Xor: {
            if (gate.inputs.size() != 2) {
                throw std::invalid_argument("EMP XOR gate expects two inputs.");
            }
            auto lhs_zero = RequireWireLabel(zero_labels, gate.inputs[0]);
            auto rhs_zero = RequireWireLabel(zero_labels, gate.inputs[1]);
            output_zero = gen.xor_gate(lhs_zero, rhs_zero);
            break;
        }
        case BitGateKind::Not: {
            if (gate.inputs.size() != 1) {
                throw std::invalid_argument("EMP NOT gate expects one input.");
            }
            // Free-NOT: evaluation flips the active label by delta, while the
            // output wire keeps the same zero label as the input wire.
            output_zero = RequireWireLabel(zero_labels, gate.inputs[0]);
            break;
        }
        case BitGateKind::Output: {
            if (gate.inputs.size() != 1) {
                throw std::invalid_argument("EMP OUTPUT gate expects one input.");
            }
            output_zero = RequireWireLabel(zero_labels, gate.inputs[0]);
            break;
        }
        case BitGateKind::Mux:
            throw std::invalid_argument("EMP backend does not yet lower MUX gates.");
        }

        zero_labels[gate.output] = output_zero;
    }

    for (const auto wire : circuit.output_wires) {
        artifact.output_zero_labels.emplace(
            wire, RequireWireLabel(zero_labels, wire));
    }

    return artifact;
}

std::vector<bool> EmpGarbledCircuit::Evaluate(
    const EmpGarbledCircuitArtifact& artifact,
    const BooleanCircuit& circuit,
    const std::unordered_map<WireId, bool>& input_bits) const {
    if (artifact.input_wires.size() != circuit.input_wires.size() ||
        artifact.output_wires.size() != circuit.output_wires.size()) {
        throw std::invalid_argument("EMP artifact does not match circuit shape.");
    }

    EmpTranscriptIO eva_io(&artifact.transcript);
    emp::HalfGateEva<EmpTranscriptIO> eva(&eva_io);

    std::unordered_map<WireId, emp::block> active_labels;
    for (const auto wire : artifact.input_wires) {
        const auto input_it = input_bits.find(wire);
        if (input_it == input_bits.end()) {
            throw std::invalid_argument("Missing EMP GC input bit.");
        }
        const auto labels_it = artifact.public_input_labels.find(wire);
        if (labels_it == artifact.public_input_labels.end()) {
            throw std::invalid_argument("Missing EMP input label pair.");
        }
        active_labels.emplace(
            wire, labels_it->second.labels[input_it->second ? 1 : 0]);
    }

    for (const auto& constant : artifact.constant_labels) {
        active_labels.emplace(constant.first, constant.second);
    }

    for (const auto& gate : circuit.gates) {
        emp::block output_label;
        switch (gate.kind) {
        case BitGateKind::And: {
            if (gate.inputs.size() != 2) {
                throw std::invalid_argument("EMP AND gate expects two inputs.");
            }
            auto lhs = RequireWireLabel(active_labels, gate.inputs[0]);
            auto rhs = RequireWireLabel(active_labels, gate.inputs[1]);
            output_label = eva.and_gate(lhs, rhs);
            break;
        }
        case BitGateKind::Xor: {
            if (gate.inputs.size() != 2) {
                throw std::invalid_argument("EMP XOR gate expects two inputs.");
            }
            auto lhs = RequireWireLabel(active_labels, gate.inputs[0]);
            auto rhs = RequireWireLabel(active_labels, gate.inputs[1]);
            output_label = eva.xor_gate(lhs, rhs);
            break;
        }
        case BitGateKind::Not: {
            if (gate.inputs.size() != 1) {
                throw std::invalid_argument("EMP NOT gate expects one input.");
            }
            output_label =
                RequireWireLabel(active_labels, gate.inputs[0]) ^ artifact.delta;
            break;
        }
        case BitGateKind::Output: {
            if (gate.inputs.size() != 1) {
                throw std::invalid_argument("EMP OUTPUT gate expects one input.");
            }
            output_label = RequireWireLabel(active_labels, gate.inputs[0]);
            break;
        }
        case BitGateKind::Mux:
            throw std::invalid_argument("EMP backend does not yet lower MUX gates.");
        }

        active_labels[gate.output] = output_label;
    }

    std::vector<bool> outputs;
    outputs.reserve(artifact.output_wires.size());
    for (const auto wire : artifact.output_wires) {
        auto active = RequireWireLabel(active_labels, wire);
        const auto zero_it = artifact.output_zero_labels.find(wire);
        if (zero_it == artifact.output_zero_labels.end()) {
            throw std::invalid_argument("Missing EMP output zero label.");
        }

        if (BlockEquals(active, zero_it->second)) {
            outputs.push_back(false);
        } else if (BlockEquals(active, LabelOne(zero_it->second, artifact.delta))) {
            outputs.push_back(true);
        } else {
            throw std::invalid_argument("EMP output label could not be decoded.");
        }
    }

    return outputs;
}

#endif

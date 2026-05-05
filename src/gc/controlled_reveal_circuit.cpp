#include "controlled_reveal_circuit.h"
#include <stdexcept>
#include <utility>

namespace {

void AddGate(BitLevelCircuit& circuit, BitGateKind kind,
             std::vector<std::string> inputs, const std::string& output) {
    circuit.gates.push_back({kind, std::move(inputs), output});
}

} // namespace

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

LWECiphertext ControlledRevealCircuit::EvalLessOrEqualPredicate(
    const std::vector<LWECiphertext>& x,
    const std::vector<LWECiphertext>& bound) {
    if (x.empty() || x.size() != bound.size()) {
        throw std::invalid_argument("Predicate inputs must be non-empty and equal width.");
    }

    auto not_bound0 = fhe_gates.EvalNOT(bound[0]);
    LWECiphertext greater = fhe_gates.EvalAND(not_bound0, x[0]);

    for (size_t i = 1; i < x.size(); ++i) {
        auto not_bound_i = fhe_gates.EvalNOT(bound[i]);
        auto generate = fhe_gates.EvalAND(not_bound_i, x[i]);
        auto xor_bits = fhe_gates.EvalXOR(bound[i], x[i]);
        auto equal_bits = fhe_gates.EvalNOT(xor_bits);
        auto propagate = fhe_gates.EvalAND(equal_bits, greater);

        // generate and propagate are mutually exclusive for this comparator bit.
        greater = fhe_gates.EvalXOR(generate, propagate);
    }

    return fhe_gates.EvalNOT(greater);
}

bool ControlledRevealCircuit::RevealPredicateOnly(const LWECiphertext& predicate_ct) {
    LWEPlaintext predicate_bit;
    fhe_ctx.GetContext().Decrypt(fhe_ctx.GetSecretKey(), predicate_ct, &predicate_bit);
    return predicate_bit == 1;
}

bool ControlledRevealCircuit::Evaluate(const std::vector<LWECiphertext>& x,
                                       const std::vector<LWECiphertext>& bound) {
    auto predicate_ct = EvalLessOrEqualPredicate(x, bound);
    return RevealPredicateOnly(predicate_ct);
}

PredicateGCArtifact ControlledRevealCircuit::ArtifactInfo(size_t bit_length) const {
    auto circuit = DescribeLessOrEqualCircuit(bit_length);
    return {circuit.name, circuit.input_bit_length, circuit.gates.size()};
}

BitLevelCircuit ControlledRevealCircuit::DescribeLessOrEqualCircuit(size_t bit_length) const {
    if (bit_length == 0) {
        throw std::invalid_argument("Circuit bit length must be greater than zero.");
    }

    BitLevelCircuit circuit;
    circuit.name = "g(c_x,c_b)=Dec(Eval([x<=b],c_x,c_b))";
    circuit.input_bit_length = bit_length;

    for (size_t i = 0; i < bit_length; ++i) {
        circuit.input_wires.push_back("x_" + std::to_string(i));
        circuit.input_wires.push_back("b_" + std::to_string(i));
    }

    AddGate(circuit, BitGateKind::Not, {"b_0"}, "not_b_0");
    AddGate(circuit, BitGateKind::And, {"not_b_0", "x_0"}, "gt_0");

    std::string greater = "gt_0";
    for (size_t i = 1; i < bit_length; ++i) {
        const auto idx = std::to_string(i);
        AddGate(circuit, BitGateKind::Not, {"b_" + idx}, "not_b_" + idx);
        AddGate(circuit, BitGateKind::And, {"not_b_" + idx, "x_" + idx}, "gen_" + idx);
        AddGate(circuit, BitGateKind::Xor, {"b_" + idx, "x_" + idx}, "xor_" + idx);
        AddGate(circuit, BitGateKind::Not, {"xor_" + idx}, "eq_" + idx);
        AddGate(circuit, BitGateKind::And, {"eq_" + idx, greater}, "prop_" + idx);
        AddGate(circuit, BitGateKind::Xor, {"gen_" + idx, "prop_" + idx}, "gt_" + idx);
        greater = "gt_" + idx;
    }

    AddGate(circuit, BitGateKind::Not, {greater}, "le");
    AddGate(circuit, BitGateKind::Output, {"le"}, "predicate_bit");
    circuit.output_wire = "predicate_bit";

    return circuit;
}

#include "controlled_reveal_circuit.h"
#include <array>
#include <stdexcept>

namespace {

constexpr std::array<bool, 4> kFixedMockHsk = {true, false, true, true};
constexpr std::array<bool, 4> kFixedMockCipherMask = {true, true, true, true};

bool InnerProductMod2(const std::array<bool, 4>& lhs,
                      const std::array<bool, 4>& rhs) {
    bool parity = false;
    for (size_t i = 0; i < lhs.size(); ++i) {
        parity = parity != (lhs[i] && rhs[i]);
    }
    return parity;
}

WireId XorReduce(BooleanCircuitBuilder& builder,
                 const std::vector<WireId>& wires,
                 const std::string& name_prefix) {
    if (wires.empty()) {
        throw std::invalid_argument("Cannot XOR-reduce an empty wire set.");
    }

    WireId result = wires[0];
    for (size_t i = 1; i < wires.size(); ++i) {
        result = builder.AddGate(
            BitGateKind::Xor, {result, wires[i]},
            name_prefix + "_" + std::to_string(i));
    }
    return result;
}

} // namespace

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

BooleanCircuit ControlledRevealCircuit::DescribeLessOrEqualCircuit(size_t bit_length) const {
    if (bit_length == 0) {
        throw std::invalid_argument("Circuit bit length must be greater than zero.");
    }

    BooleanCircuitBuilder builder(
        "g(c_x,c_b)=Dec(Eval([x<=b],c_x,c_b))", bit_length);
    std::vector<WireId> x_wires;
    std::vector<WireId> bound_wires;

    for (size_t i = 0; i < bit_length; ++i) {
        const auto idx = std::to_string(i);
        x_wires.push_back(builder.AddInputWire("x_" + idx));
        bound_wires.push_back(builder.AddInputWire("b_" + idx));
    }

    auto not_bound = builder.AddGate(BitGateKind::Not, {bound_wires[0]}, "not_b_0");
    WireId greater = builder.AddGate(BitGateKind::And, {not_bound, x_wires[0]}, "gt_0");

    for (size_t i = 1; i < bit_length; ++i) {
        const auto idx = std::to_string(i);
        not_bound = builder.AddGate(BitGateKind::Not, {bound_wires[i]}, "not_b_" + idx);
        auto generate = builder.AddGate(BitGateKind::And, {not_bound, x_wires[i]}, "gen_" + idx);
        auto xor_bits = builder.AddGate(BitGateKind::Xor, {bound_wires[i], x_wires[i]}, "xor_" + idx);
        auto equal_bits = builder.AddGate(BitGateKind::Not, {xor_bits}, "eq_" + idx);
        auto propagate = builder.AddGate(BitGateKind::And, {equal_bits, greater}, "prop_" + idx);
        greater = builder.AddGate(BitGateKind::Xor, {generate, propagate}, "gt_" + idx);
    }

    auto predicate_msg = builder.AddGate(BitGateKind::Not, {greater}, "predicate_msg");
    const bool mock_eval_pad =
        InnerProductMod2(kFixedMockCipherMask, kFixedMockHsk);
    auto mock_eval_pad_bit =
        builder.AddConstantWire("mock_eval_pad_bit", mock_eval_pad);
    auto predicate_ct_body = builder.AddGate(
        BitGateKind::Xor, {predicate_msg, mock_eval_pad_bit},
        "mock_predicate_ct_body");

    std::vector<WireId> masked_hsk_terms;
    for (size_t i = 0; i < kFixedMockHsk.size(); ++i) {
        const auto idx = std::to_string(i);
        auto mask_bit = builder.AddConstantWire(
            "mock_ct_mask_" + idx, kFixedMockCipherMask[i]);
        auto hsk_bit = builder.AddConstantWire(
            "hardcoded_mock_hsk_" + idx, kFixedMockHsk[i]);
        masked_hsk_terms.push_back(builder.AddGate(
            BitGateKind::And, {mask_bit, hsk_bit}, "mock_dec_term_" + idx));
    }

    auto mock_dec_pad = XorReduce(builder, masked_hsk_terms, "mock_dec_pad");
    auto decrypted_predicate = builder.AddGate(
        BitGateKind::Xor, {predicate_ct_body, mock_dec_pad}, "mock_dec_out");
    auto predicate = builder.AddGate(
        BitGateKind::Output, {decrypted_predicate}, "predicate_bit");
    builder.AddOutputWire(predicate);

    return builder.Build();
}

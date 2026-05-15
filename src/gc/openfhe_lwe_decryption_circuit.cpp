#include "openfhe_lwe_decryption_circuit.h"
#include <stdexcept>

namespace {

WireId BuildOr(BooleanCircuitBuilder& builder, WireId lhs, WireId rhs,
               const std::string& name) {
    auto xor_wire = builder.AddGate(BitGateKind::Xor, {lhs, rhs}, name + "_xor");
    auto and_wire = builder.AddGate(BitGateKind::And, {lhs, rhs}, name + "_and");
    return builder.AddGate(BitGateKind::Xor, {xor_wire, and_wire}, name);
}

} // namespace

const std::array<bool, OpenFHELWEDecryptionCircuit::kDemoDimension>&
OpenFHELWEDecryptionCircuit::FixedDemoSecretKey() {
    static constexpr std::array<bool, kDemoDimension> key = {true, false, true, true};
    return key;
}

const std::array<uint64_t, OpenFHELWEDecryptionCircuit::kDemoDimension>&
OpenFHELWEDecryptionCircuit::FixedDemoMask() {
    static constexpr std::array<uint64_t, kDemoDimension> mask = {3, 5, 6, 1};
    return mask;
}

WireId OpenFHELWEDecryptionCircuit::BuildDemoPredicateDecrypt(
    BooleanCircuitBuilder& builder,
    WireId predicate_msg) {
    const auto& fixed_hsk = FixedDemoSecretKey();
    const auto& fixed_mask = FixedDemoMask();

    std::vector<WireId> hsk_bits;
    std::vector<std::vector<WireId>> mask_coefficients;
    for (size_t i = 0; i < kDemoDimension; ++i) {
        const auto idx = std::to_string(i);
        hsk_bits.push_back(
            builder.AddConstantWire("hardcoded_openfhe_lwe_hsk_" + idx,
                                    fixed_hsk[i]));
        mask_coefficients.push_back(
            ConstantBits(builder, fixed_mask[i], "openfhe_lwe_a_" + idx));
    }

    auto decrypt_pad = DotProductModQ(
        builder, mask_coefficients, hsk_bits, "openfhe_lwe_dec_pad");

    std::vector<WireId> encoded_msg =
        ZeroBits(builder, "openfhe_lwe_encoded_msg_zero");
    encoded_msg[2] = predicate_msg;

    auto ct_body_bits = AddModulo(
        builder, encoded_msg, decrypt_pad,
        builder.AddConstantWire("openfhe_lwe_eval_carry_zero", false),
        "openfhe_lwe_ct_body");

    auto recomputed_pad = DotProductModQ(
        builder, mask_coefficients, hsk_bits, "openfhe_lwe_recomputed_pad");
    auto phase_bits = SubtractModulo(
        builder, ct_body_bits, recomputed_pad, "openfhe_lwe_phase");

    // OpenFHE LWE decrypt computes floor(p * (phase + q/(2p)) / q).
    // For q=16 and p=4, the output bit is bit 2 after adding the rounding offset.
    auto rounding_offset = ConstantBits(
        builder,
        kDemoModulus / (kDemoPlaintextModulus * 2),
        "openfhe_lwe_rounding_offset");
    auto rounding_carry_zero = builder.AddConstantWire(
        "openfhe_lwe_rounding_carry_zero", false);
    auto rounded_phase = AddModulo(
        builder, phase_bits, rounding_offset, rounding_carry_zero,
        "openfhe_lwe_rounded_phase");
    return rounded_phase[2];
}

std::vector<WireId> OpenFHELWEDecryptionCircuit::ConstantBits(
    BooleanCircuitBuilder& builder,
    uint64_t value,
    const std::string& prefix) {
    std::vector<WireId> bits;
    bits.reserve(kDemoModulusBits);
    for (size_t i = 0; i < kDemoModulusBits; ++i) {
        bits.push_back(builder.AddConstantWire(
            prefix + "_bit_" + std::to_string(i), ((value >> i) & 1U) != 0));
    }
    return bits;
}

std::vector<WireId> OpenFHELWEDecryptionCircuit::ZeroBits(
    BooleanCircuitBuilder& builder,
    const std::string& prefix) {
    return ConstantBits(builder, 0, prefix);
}

std::vector<WireId> OpenFHELWEDecryptionCircuit::AddModulo(
    BooleanCircuitBuilder& builder,
    const std::vector<WireId>& lhs,
    const std::vector<WireId>& rhs,
    WireId carry_in,
    const std::string& prefix) {
    if (lhs.size() != rhs.size() || lhs.size() != kDemoModulusBits) {
        throw std::invalid_argument("AddModulo expects fixed-width operands.");
    }

    std::vector<WireId> sum;
    sum.reserve(lhs.size());
    WireId carry = carry_in;

    for (size_t i = 0; i < lhs.size(); ++i) {
        const auto idx = std::to_string(i);
        auto axorb = builder.AddGate(
            BitGateKind::Xor, {lhs[i], rhs[i]}, prefix + "_xor_ab_" + idx);
        sum.push_back(builder.AddGate(
            BitGateKind::Xor, {axorb, carry}, prefix + "_sum_" + idx));

        auto generate = builder.AddGate(
            BitGateKind::And, {lhs[i], rhs[i]}, prefix + "_carry_gen_" + idx);
        auto propagate = builder.AddGate(
            BitGateKind::And, {axorb, carry}, prefix + "_carry_prop_" + idx);
        carry = BuildOr(builder, generate, propagate, prefix + "_carry_" + idx);
    }

    return sum;
}

std::vector<WireId> OpenFHELWEDecryptionCircuit::SubtractModulo(
    BooleanCircuitBuilder& builder,
    const std::vector<WireId>& lhs,
    const std::vector<WireId>& rhs,
    const std::string& prefix) {
    if (lhs.size() != rhs.size() || lhs.size() != kDemoModulusBits) {
        throw std::invalid_argument("SubtractModulo expects fixed-width operands.");
    }

    std::vector<WireId> inverted_rhs;
    inverted_rhs.reserve(rhs.size());
    for (size_t i = 0; i < rhs.size(); ++i) {
        inverted_rhs.push_back(builder.AddGate(
            BitGateKind::Not, {rhs[i]}, prefix + "_not_rhs_" + std::to_string(i)));
    }

    auto one = builder.AddConstantWire(prefix + "_carry_one", true);
    return AddModulo(builder, lhs, inverted_rhs, one, prefix);
}

std::vector<WireId> OpenFHELWEDecryptionCircuit::SelectBySecretBit(
    BooleanCircuitBuilder& builder,
    const std::vector<WireId>& value,
    WireId secret_bit,
    const std::string& prefix) {
    std::vector<WireId> selected;
    selected.reserve(value.size());
    for (size_t i = 0; i < value.size(); ++i) {
        selected.push_back(builder.AddGate(
            BitGateKind::And, {value[i], secret_bit},
            prefix + "_selected_" + std::to_string(i)));
    }
    return selected;
}

std::vector<WireId> OpenFHELWEDecryptionCircuit::DotProductModQ(
    BooleanCircuitBuilder& builder,
    const std::vector<std::vector<WireId>>& mask_coefficients,
    const std::vector<WireId>& hsk_bits,
    const std::string& prefix) {
    if (mask_coefficients.size() != hsk_bits.size()) {
        throw std::invalid_argument("DotProductModQ input sizes must match.");
    }

    auto total = ZeroBits(builder, prefix + "_zero");
    auto carry_zero = builder.AddConstantWire(prefix + "_carry_zero", false);

    for (size_t i = 0; i < mask_coefficients.size(); ++i) {
        auto selected = SelectBySecretBit(
            builder, mask_coefficients[i], hsk_bits[i],
            prefix + "_term_" + std::to_string(i));
        total = AddModulo(
            builder, total, selected, carry_zero,
            prefix + "_acc_" + std::to_string(i));
    }

    return total;
}

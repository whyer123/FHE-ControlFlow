#include "openfhe_eval_bin_gate_circuit.h"
#include "src/gc/openfhe_lwe_decryption_circuit.h"

#include <stdexcept>

namespace {

constexpr size_t kDimension = OpenFHELWEDecryptionCircuit::kDemoDimension;
constexpr size_t kModulusBits = OpenFHELWEDecryptionCircuit::kDemoModulusBits;

std::string GateName(OpenFHEBinGateKind gate) {
    switch (gate) {
    case OpenFHEBinGateKind::And:
        return "AND";
    case OpenFHEBinGateKind::Or:
        return "OR";
    case OpenFHEBinGateKind::Xor:
        return "XOR";
    case OpenFHEBinGateKind::Xnor:
        return "XNOR";
    }
    throw std::invalid_argument("unknown OpenFHE binary gate");
}

WireId BuildOr(BooleanCircuitBuilder& builder, WireId lhs, WireId rhs,
               const std::string& name) {
    auto xor_wire = builder.AddGate(BitGateKind::Xor, {lhs, rhs}, name + "_xor");
    auto and_wire = builder.AddGate(BitGateKind::And, {lhs, rhs}, name + "_and");
    return builder.AddGate(BitGateKind::Xor, {xor_wire, and_wire}, name);
}

std::vector<WireId> ConstantBits(BooleanCircuitBuilder& builder,
                                 uint64_t value,
                                 const std::string& prefix) {
    std::vector<WireId> bits;
    bits.reserve(kModulusBits);
    for (size_t i = 0; i < kModulusBits; ++i) {
        bits.push_back(builder.AddConstantWire(
            prefix + "_bit_" + std::to_string(i), ((value >> i) & 1U) != 0));
    }
    return bits;
}

std::vector<WireId> ZeroBits(BooleanCircuitBuilder& builder,
                             const std::string& prefix) {
    return ConstantBits(builder, 0, prefix);
}

std::vector<WireId> AddModulo(BooleanCircuitBuilder& builder,
                              const std::vector<WireId>& lhs,
                              const std::vector<WireId>& rhs,
                              WireId carry_in,
                              const std::string& prefix) {
    if (lhs.size() != rhs.size() || lhs.size() != kModulusBits) {
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

std::vector<WireId> SubtractModulo(BooleanCircuitBuilder& builder,
                                   const std::vector<WireId>& lhs,
                                   const std::vector<WireId>& rhs,
                                   const std::string& prefix) {
    if (lhs.size() != rhs.size() || lhs.size() != kModulusBits) {
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

std::vector<WireId> SelectBySecretBit(BooleanCircuitBuilder& builder,
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

std::vector<WireId> FixedHskBits(BooleanCircuitBuilder& builder,
                                 const std::string& prefix) {
    const auto& hsk = OpenFHELWEDecryptionCircuit::FixedDemoSecretKey();
    std::vector<WireId> bits;
    bits.reserve(hsk.size());
    for (size_t i = 0; i < hsk.size(); ++i) {
        bits.push_back(builder.AddConstantWire(
            prefix + "_hsk_" + std::to_string(i), hsk[i]));
    }
    return bits;
}

std::vector<WireId> DotProductModQ(
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

std::vector<WireId> PhaseBits(BooleanCircuitBuilder& builder,
                              const DemoLWECiphertextWireBits& ct,
                              const std::string& prefix) {
    const auto hsk_bits = FixedHskBits(builder, prefix);
    const auto pad = DotProductModQ(
        builder, ct.mask_coefficients, hsk_bits, prefix + "_pad");
    return SubtractModulo(builder, ct.body_bits, pad, prefix + "_phase");
}

WireId BootstrapPlaceholderLut(BooleanCircuitBuilder& builder,
                               OpenFHEBinGateKind gate,
                               const std::vector<WireId>& phase_bits,
                               const std::string& prefix) {
    if (phase_bits.size() != kModulusBits) {
        throw std::invalid_argument("bootstrap placeholder expects q-bit phase.");
    }

    switch (gate) {
    case OpenFHEBinGateKind::And:
        return builder.AddGate(
            BitGateKind::Output, {phase_bits[3]},
            prefix + "_and_lut_msg");
    case OpenFHEBinGateKind::Or:
        return BuildOr(builder, phase_bits[2], phase_bits[3],
                       prefix + "_or_lut_msg");
    case OpenFHEBinGateKind::Xor:
        return builder.AddGate(
            BitGateKind::Output, {phase_bits[3]},
            prefix + "_xor_lut_msg");
    case OpenFHEBinGateKind::Xnor: {
        auto xor_msg = builder.AddGate(
            BitGateKind::Output, {phase_bits[3]},
            prefix + "_xnor_lut_xor_msg");
        return builder.AddGate(BitGateKind::Not, {xor_msg},
                               prefix + "_xnor_lut_msg");
    }
    }

    throw std::invalid_argument("unsupported bootstrap placeholder gate");
}

DemoLWECiphertextWireBits FixedMaskEncryptBit(BooleanCircuitBuilder& builder,
                                              WireId message_bit,
                                              const std::string& prefix) {
    const auto& fixed_mask = OpenFHELWEDecryptionCircuit::FixedDemoMask();
    DemoLWECiphertextWireBits ct;
    ct.mask_coefficients.reserve(fixed_mask.size());
    for (size_t i = 0; i < fixed_mask.size(); ++i) {
        ct.mask_coefficients.push_back(
            ConstantBits(builder, fixed_mask[i],
                         prefix + "_a_" + std::to_string(i)));
    }

    const auto hsk_bits = FixedHskBits(builder, prefix + "_body");
    const auto pad = DotProductModQ(
        builder, ct.mask_coefficients, hsk_bits, prefix + "_body_pad");
    auto encoded_msg = ZeroBits(builder, prefix + "_encoded_msg_zero");
    encoded_msg[2] = message_bit;
    const auto carry_zero =
        builder.AddConstantWire(prefix + "_body_carry_zero", false);
    ct.body_bits = AddModulo(builder, encoded_msg, pad, carry_zero,
                             prefix + "_body");
    return ct;
}

DemoLWECiphertextWireBits AddCiphertexts(BooleanCircuitBuilder& builder,
                                         const DemoLWECiphertextWireBits& lhs,
                                         const DemoLWECiphertextWireBits& rhs,
                                         const std::string& prefix) {
    if (lhs.mask_coefficients.size() != rhs.mask_coefficients.size() ||
        lhs.body_bits.size() != rhs.body_bits.size()) {
        throw std::invalid_argument("ciphertext sizes must match.");
    }

    DemoLWECiphertextWireBits result;
    result.mask_coefficients.reserve(lhs.mask_coefficients.size());
    const auto carry_zero =
        builder.AddConstantWire(prefix + "_carry_zero", false);

    for (size_t i = 0; i < lhs.mask_coefficients.size(); ++i) {
        result.mask_coefficients.push_back(AddModulo(
            builder, lhs.mask_coefficients[i], rhs.mask_coefficients[i],
            carry_zero, prefix + "_a_" + std::to_string(i)));
    }
    result.body_bits = AddModulo(
        builder, lhs.body_bits, rhs.body_bits, carry_zero, prefix + "_b");

    return result;
}

DemoLWECiphertextWireBits DoubleCiphertext(BooleanCircuitBuilder& builder,
                                           const DemoLWECiphertextWireBits& ct,
                                           const std::string& prefix) {
    return AddCiphertexts(builder, ct, ct, prefix);
}

void AddCiphertextOutputs(BooleanCircuitBuilder& builder,
                          const DemoLWECiphertextWireBits& ct,
                          const std::string& prefix) {
    for (size_t i = 0; i < ct.mask_coefficients.size(); ++i) {
        for (size_t bit = 0; bit < ct.mask_coefficients[i].size(); ++bit) {
            const auto output = builder.AddGate(
                BitGateKind::Output, {ct.mask_coefficients[i][bit]},
                prefix + "_a_" + std::to_string(i) + "_bit_" +
                    std::to_string(bit));
            builder.AddOutputWire(output);
        }
    }

    for (size_t bit = 0; bit < ct.body_bits.size(); ++bit) {
        const auto output = builder.AddGate(
            BitGateKind::Output, {ct.body_bits[bit]},
            prefix + "_b_bit_" + std::to_string(bit));
        builder.AddOutputWire(output);
    }
}

} // namespace

BooleanCircuit OpenFHEEvalBinGateCircuit::DescribeDemoEvalBinGate(
    OpenFHEBinGateKind gate) {
    constexpr size_t ciphertext_bits = kDimension * kModulusBits + kModulusBits;
    BooleanCircuitBuilder builder(
        "OpenFHE.EvalBinGate." + GateName(gate) +
            ".demo_lwe_boolean_expansion",
        ciphertext_bits * 2);
    const auto lhs = AddDemoCiphertextInput(builder, "lhs");
    const auto rhs = AddDemoCiphertextInput(builder, "rhs");
    const auto output = BuildDemoEvalBinGate(
        builder, gate, lhs, rhs, "openfhe_evalbingate");
    AddCiphertextOutputs(builder, output, "evalbingate_out");
    return builder.Build();
}

DemoLWECiphertextWireBits OpenFHEEvalBinGateCircuit::AddDemoCiphertextInput(
    BooleanCircuitBuilder& builder,
    const std::string& prefix) {
    DemoLWECiphertextWireBits ct;
    ct.mask_coefficients.reserve(kDimension);
    for (size_t i = 0; i < kDimension; ++i) {
        std::vector<WireId> coefficient;
        coefficient.reserve(kModulusBits);
        for (size_t bit = 0; bit < kModulusBits; ++bit) {
            coefficient.push_back(builder.AddInputWire(
                prefix + "_a_" + std::to_string(i) + "_bit_" +
                std::to_string(bit)));
        }
        ct.mask_coefficients.push_back(std::move(coefficient));
    }

    ct.body_bits.reserve(kModulusBits);
    for (size_t bit = 0; bit < kModulusBits; ++bit) {
        ct.body_bits.push_back(builder.AddInputWire(
            prefix + "_b_bit_" + std::to_string(bit)));
    }

    return ct;
}

DemoLWECiphertextWireBits OpenFHEEvalBinGateCircuit::BuildDemoEvalBinGate(
    BooleanCircuitBuilder& builder,
    OpenFHEBinGateKind gate,
    const DemoLWECiphertextWireBits& lhs,
    const DemoLWECiphertextWireBits& rhs,
    const std::string& prefix) {
    auto prebootstrap = AddCiphertexts(
        builder, lhs, rhs, prefix + "_prebootstrap_add");
    if (gate == OpenFHEBinGateKind::Xor ||
        gate == OpenFHEBinGateKind::Xnor) {
        prebootstrap = DoubleCiphertext(
            builder, prebootstrap, prefix + "_prebootstrap_double");
    }

    const auto phase = PhaseBits(
        builder, prebootstrap, prefix + "_openfhe_bootstrap_placeholder");
    const auto message = BootstrapPlaceholderLut(
        builder, gate, phase, prefix + "_openfhe_bootstrap_placeholder");
    return FixedMaskEncryptBit(
        builder, message, prefix + "_openfhe_bootstrap_placeholder_out");
}

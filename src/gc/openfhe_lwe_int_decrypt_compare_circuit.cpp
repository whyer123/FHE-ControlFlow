#include "openfhe_lwe_int_decrypt_compare_circuit.h"

#include <stdexcept>
#include <utility>

namespace {

void Require(bool condition, const std::string& message) {
    if (!condition) {
        throw std::invalid_argument(message);
    }
}

bool IsPowerOfTwo(uint64_t value) {
    return value != 0 && (value & (value - 1U)) == 0;
}

WireId BuildOr(BooleanCircuitBuilder& builder, WireId lhs, WireId rhs,
               const std::string& name) {
    auto xor_wire = builder.AddGate(BitGateKind::Xor, {lhs, rhs},
                                    name + "_xor");
    auto and_wire = builder.AddGate(BitGateKind::And, {lhs, rhs},
                                    name + "_and");
    return builder.AddGate(BitGateKind::Xor, {xor_wire, and_wire}, name);
}

std::vector<WireId> ConstantBits(BooleanCircuitBuilder& builder,
                                 size_t bit_width,
                                 uint64_t value,
                                 const std::string& prefix) {
    std::vector<WireId> bits;
    bits.reserve(bit_width);
    for (size_t i = 0; i < bit_width; ++i) {
        bits.push_back(builder.AddConstantWire(
            prefix + "_bit_" + std::to_string(i),
            ((value >> i) & 1ULL) != 0));
    }
    return bits;
}

std::vector<WireId> ZeroBits(BooleanCircuitBuilder& builder,
                             size_t bit_width,
                             const std::string& prefix) {
    return ConstantBits(builder, bit_width, 0, prefix);
}

std::vector<WireId> AddModulo(BooleanCircuitBuilder& builder,
                              const std::vector<WireId>& lhs,
                              const std::vector<WireId>& rhs,
                              WireId carry_in,
                              const std::string& prefix) {
    Require(lhs.size() == rhs.size(), "AddModulo width mismatch.");

    std::vector<WireId> sum;
    sum.reserve(lhs.size());
    WireId carry = carry_in;

    for (size_t i = 0; i < lhs.size(); ++i) {
        const auto idx = std::to_string(i);
        auto axorb = builder.AddGate(BitGateKind::Xor, {lhs[i], rhs[i]},
                                     prefix + "_xor_ab_" + idx);
        sum.push_back(builder.AddGate(BitGateKind::Xor, {axorb, carry},
                                      prefix + "_sum_" + idx));

        auto generate = builder.AddGate(BitGateKind::And, {lhs[i], rhs[i]},
                                        prefix + "_carry_gen_" + idx);
        auto propagate = builder.AddGate(BitGateKind::And, {axorb, carry},
                                         prefix + "_carry_prop_" + idx);
        carry = BuildOr(builder, generate, propagate,
                        prefix + "_carry_" + idx);
    }

    return sum;
}

std::vector<WireId> SubtractModulo(BooleanCircuitBuilder& builder,
                                   const std::vector<WireId>& lhs,
                                   const std::vector<WireId>& rhs,
                                   const std::string& prefix) {
    Require(lhs.size() == rhs.size(), "SubtractModulo width mismatch.");

    std::vector<WireId> inverted_rhs;
    inverted_rhs.reserve(rhs.size());
    for (size_t i = 0; i < rhs.size(); ++i) {
        inverted_rhs.push_back(builder.AddGate(
            BitGateKind::Not, {rhs[i]}, prefix + "_not_rhs_" +
                                          std::to_string(i)));
    }

    auto one = builder.AddConstantWire(prefix + "_carry_one", true);
    return AddModulo(builder, lhs, inverted_rhs, one, prefix);
}

std::vector<WireId> SelectVector(BooleanCircuitBuilder& builder,
                                 WireId select,
                                 const std::vector<WireId>& if_true,
                                 const std::vector<WireId>& if_false,
                                 const std::string& prefix) {
    Require(if_true.size() == if_false.size(), "SelectVector width mismatch.");

    auto not_select = builder.AddGate(BitGateKind::Not, {select},
                                      prefix + "_not_select");
    std::vector<WireId> out;
    out.reserve(if_true.size());
    for (size_t i = 0; i < if_true.size(); ++i) {
        const auto idx = std::to_string(i);
        auto selected_true = builder.AddGate(
            BitGateKind::And, {select, if_true[i]},
            prefix + "_true_" + idx);
        auto selected_false = builder.AddGate(
            BitGateKind::And, {not_select, if_false[i]},
            prefix + "_false_" + idx);
        out.push_back(builder.AddGate(BitGateKind::Xor,
                                      {selected_true, selected_false},
                                      prefix + "_out_" + idx));
    }
    return out;
}

std::vector<WireId> NegateModulo(BooleanCircuitBuilder& builder,
                                 const std::vector<WireId>& value,
                                 const std::string& prefix) {
    return SubtractModulo(
        builder, ZeroBits(builder, value.size(), prefix + "_zero"), value,
        prefix);
}

std::vector<WireId> BuildSecretCoefficientTerm(
    BooleanCircuitBuilder& builder,
    const OpenFHELWEIntCircuitParams& params,
    const std::vector<WireId>& value,
    uint64_t coefficient,
    const std::string& prefix) {
    Require(value.size() == params.modulus_bits,
            "secret coefficient term width mismatch.");

    const bool nonzero = (coefficient % params.ciphertext_modulus) != 0;
    const bool negative =
        coefficient == params.ciphertext_modulus - 1U;

    auto secret_nonzero = builder.AddSecretConstantWire(
        prefix + "_hsk_nonzero", nonzero);
    auto secret_negative = builder.AddSecretConstantWire(
        prefix + "_hsk_negative", negative);

    auto negated = NegateModulo(builder, value, prefix + "_neg");
    auto signed_term = SelectVector(builder, secret_negative, negated, value,
                                    prefix + "_signed");
    auto zero = ZeroBits(builder, params.modulus_bits, prefix + "_zero");
    return SelectVector(builder, secret_nonzero, signed_term, zero,
                        prefix + "_nonzero");
}

std::vector<WireId> DotProductWithSecret(
    BooleanCircuitBuilder& builder,
    const OpenFHELWEIntCircuitParams& params,
    const std::vector<std::vector<WireId>>& a_bits,
    const std::string& prefix) {
    Require(a_bits.size() == params.hsk_mod_q.size(),
            "DotProductWithSecret dimension mismatch.");

    auto total = ZeroBits(builder, params.modulus_bits, prefix + "_zero");
    auto carry_zero = builder.AddConstantWire(prefix + "_carry_zero", false);
    for (size_t i = 0; i < a_bits.size(); ++i) {
        auto term = BuildSecretCoefficientTerm(
            builder, params, a_bits[i], params.hsk_mod_q[i],
            prefix + "_term_" + std::to_string(i));
        total = AddModulo(builder, total, term, carry_zero,
                          prefix + "_acc_" + std::to_string(i));
    }
    return total;
}

void ValidateParams(const OpenFHELWEIntCircuitParams& params) {
    Require(params.dimension > 0, "dimension must be positive.");
    Require(params.modulus_bits > 0, "modulus_bits must be positive.");
    Require(params.ciphertext_modulus > 0, "ciphertext modulus is zero.");
    Require(IsPowerOfTwo(params.ciphertext_modulus),
            "first v2 circuit requires power-of-two q.");
    Require((1ULL << params.modulus_bits) == params.ciphertext_modulus,
            "modulus_bits must match q.");
    Require(params.plaintext_modulus > 0, "plaintext modulus is zero.");
    Require(IsPowerOfTwo(params.plaintext_modulus),
            "first v2 circuit requires power-of-two p.");
    Require((1ULL << params.plaintext_bits) == params.plaintext_modulus,
            "plaintext_bits must match p.");
    Require(params.ciphertext_modulus % (params.plaintext_modulus * 2U) == 0,
            "q must be divisible by 2p for OpenFHE rounding.");
    Require(params.hsk_mod_q.size() == params.dimension,
            "hsk dimension mismatch.");
    for (const auto coefficient : params.hsk_mod_q) {
        Require(coefficient == 0 || coefficient == 1 ||
                    coefficient == params.ciphertext_modulus - 1U,
                "first v2 circuit supports ternary hsk coefficients only.");
    }
}

} // namespace

BooleanCircuit OpenFHELWEIntDecryptCompareCircuit::Describe(
    const OpenFHELWEIntCircuitParams& params) {
    ValidateParams(params);

    BooleanCircuitBuilder builder(
        "OpenFHE.LWEInt.DecCompare", 2U * (params.dimension + 1U) *
                                         params.modulus_bits);

    auto lhs = AddCiphertextInput(builder, params, "lhs");
    auto rhs = AddCiphertextInput(builder, params, "rhs");
    auto lhs_plain = BuildDecrypt(builder, params, lhs, "lhs_dec");
    auto rhs_plain = BuildDecrypt(builder, params, rhs, "rhs_dec");
    auto predicate = BuildLessOrEqual(builder, lhs_plain, rhs_plain,
                                      "dec_compare");
    builder.AddOutputWire(predicate);
    return builder.Build();
}

OpenFHELWEIntCiphertextWireBits
OpenFHELWEIntDecryptCompareCircuit::AddCiphertextInput(
    BooleanCircuitBuilder& builder,
    const OpenFHELWEIntCircuitParams& params,
    const std::string& prefix) {
    ValidateParams(params);

    OpenFHELWEIntCiphertextWireBits ciphertext;
    ciphertext.a_bits.reserve(params.dimension);
    for (size_t i = 0; i < params.dimension; ++i) {
        std::vector<WireId> coefficient;
        coefficient.reserve(params.modulus_bits);
        for (size_t bit = 0; bit < params.modulus_bits; ++bit) {
            coefficient.push_back(builder.AddInputWire(
                prefix + "_a_" + std::to_string(i) + "_bit_" +
                std::to_string(bit)));
        }
        ciphertext.a_bits.push_back(std::move(coefficient));
    }

    ciphertext.body_bits.reserve(params.modulus_bits);
    for (size_t bit = 0; bit < params.modulus_bits; ++bit) {
        ciphertext.body_bits.push_back(builder.AddInputWire(
            prefix + "_body_bit_" + std::to_string(bit)));
    }
    return ciphertext;
}

std::vector<WireId> OpenFHELWEIntDecryptCompareCircuit::BuildDecrypt(
    BooleanCircuitBuilder& builder,
    const OpenFHELWEIntCircuitParams& params,
    const OpenFHELWEIntCiphertextWireBits& ciphertext,
    const std::string& prefix) {
    ValidateParams(params);
    Require(ciphertext.a_bits.size() == params.dimension,
            "ciphertext input dimension mismatch.");
    Require(ciphertext.body_bits.size() == params.modulus_bits,
            "ciphertext body width mismatch.");

    auto pad = DotProductWithSecret(builder, params, ciphertext.a_bits,
                                    prefix + "_pad");
    auto phase = SubtractModulo(builder, ciphertext.body_bits, pad,
                                prefix + "_phase");
    const uint64_t rounding_offset =
        params.ciphertext_modulus / (2U * params.plaintext_modulus);
    auto rounded = AddModulo(
        builder, phase,
        ConstantBits(builder, params.modulus_bits, rounding_offset,
                     prefix + "_rounding_offset"),
        builder.AddConstantWire(prefix + "_rounding_carry_zero", false),
        prefix + "_rounded");

    const size_t shift = params.modulus_bits - params.plaintext_bits;
    std::vector<WireId> message_bits;
    message_bits.reserve(params.plaintext_bits);
    for (size_t i = 0; i < params.plaintext_bits; ++i) {
        message_bits.push_back(rounded.at(shift + i));
    }
    return message_bits;
}

WireId OpenFHELWEIntDecryptCompareCircuit::BuildLessOrEqual(
    BooleanCircuitBuilder& builder,
    const std::vector<WireId>& lhs_bits,
    const std::vector<WireId>& rhs_bits,
    const std::string& prefix) {
    Require(lhs_bits.size() == rhs_bits.size(),
            "BuildLessOrEqual width mismatch.");
    Require(!lhs_bits.empty(), "BuildLessOrEqual expects at least one bit.");

    WireId greater = builder.AddConstantWire(prefix + "_gt_init", false);
    WireId prefix_equal =
        builder.AddConstantWire(prefix + "_prefix_eq_init", true);
    for (size_t offset = 0; offset < lhs_bits.size(); ++offset) {
        const size_t i = lhs_bits.size() - 1U - offset;
        const auto idx = std::to_string(i);
        auto not_rhs = builder.AddGate(BitGateKind::Not, {rhs_bits[i]},
                                       prefix + "_not_rhs_" + idx);
        auto lhs_gt_bit = builder.AddGate(BitGateKind::And,
                                          {lhs_bits[i], not_rhs},
                                          prefix + "_lhs_gt_bit_" + idx);
        auto lhs_gt_here = builder.AddGate(BitGateKind::And,
                                           {prefix_equal, lhs_gt_bit},
                                           prefix + "_lhs_gt_here_" + idx);
        auto xor_bit = builder.AddGate(BitGateKind::Xor,
                                       {lhs_bits[i], rhs_bits[i]},
                                       prefix + "_xor_" + idx);
        auto eq_here = builder.AddGate(BitGateKind::Not, {xor_bit},
                                       prefix + "_eq_here_" + idx);
        greater = BuildOr(builder, greater, lhs_gt_here,
                          prefix + "_gt_" + idx);
        prefix_equal = builder.AddGate(BitGateKind::And,
                                       {prefix_equal, eq_here},
                                       prefix + "_prefix_eq_" + idx);
    }
    return builder.AddGate(BitGateKind::Not, {greater}, prefix + "_le");
}

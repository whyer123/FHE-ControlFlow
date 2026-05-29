#pragma once

#include "src/gc/boolean_circuit.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

struct OpenFHELWEIntCircuitParams {
    size_t dimension = 0;
    size_t modulus_bits = 0;
    uint64_t ciphertext_modulus = 0;
    uint64_t plaintext_modulus = 0;
    size_t plaintext_bits = 0;
    std::vector<uint64_t> hsk_mod_q;
};

struct OpenFHELWEIntCiphertextWireBits {
    std::vector<std::vector<WireId>> a_bits;
    std::vector<WireId> body_bits;
};

class OpenFHELWEIntDecryptCompareCircuit {
public:
    static BooleanCircuit Describe(const OpenFHELWEIntCircuitParams& params);

    static OpenFHELWEIntCiphertextWireBits AddCiphertextInput(
        BooleanCircuitBuilder& builder,
        const OpenFHELWEIntCircuitParams& params,
        const std::string& prefix);

    static std::vector<WireId> BuildDecrypt(
        BooleanCircuitBuilder& builder,
        const OpenFHELWEIntCircuitParams& params,
        const OpenFHELWEIntCiphertextWireBits& ciphertext,
        const std::string& prefix);

    static WireId BuildLessOrEqual(BooleanCircuitBuilder& builder,
                                   const std::vector<WireId>& lhs_bits,
                                   const std::vector<WireId>& rhs_bits,
                                   const std::string& prefix);
};

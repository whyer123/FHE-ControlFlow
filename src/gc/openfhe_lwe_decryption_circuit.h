#pragma once

#include "src/gc/boolean_circuit.h"
#include <array>
#include <string>
#include <vector>

class OpenFHELWEDecryptionCircuit {
public:
    static constexpr size_t kDemoDimension = 4;
    static constexpr size_t kDemoModulusBits = 4;
    static constexpr uint64_t kDemoModulus = 1ULL << kDemoModulusBits;
    static constexpr uint64_t kDemoPlaintextScale = 4;

    static const std::array<bool, kDemoDimension>& FixedDemoSecretKey();
    static const std::array<uint64_t, kDemoDimension>& FixedDemoMask();

    static WireId BuildDemoPredicateDecrypt(
        BooleanCircuitBuilder& builder,
        WireId predicate_msg);

private:
    static std::vector<WireId> ConstantBits(BooleanCircuitBuilder& builder,
                                            uint64_t value,
                                            const std::string& prefix);
    static std::vector<WireId> ZeroBits(BooleanCircuitBuilder& builder,
                                        const std::string& prefix);
    static std::vector<WireId> AddModulo(BooleanCircuitBuilder& builder,
                                         const std::vector<WireId>& lhs,
                                         const std::vector<WireId>& rhs,
                                         WireId carry_in,
                                         const std::string& prefix);
    static std::vector<WireId> SubtractModulo(BooleanCircuitBuilder& builder,
                                              const std::vector<WireId>& lhs,
                                              const std::vector<WireId>& rhs,
                                              const std::string& prefix);
    static std::vector<WireId> SelectBySecretBit(BooleanCircuitBuilder& builder,
                                                 const std::vector<WireId>& value,
                                                 WireId secret_bit,
                                                 const std::string& prefix);
    static std::vector<WireId> DotProductModQ(BooleanCircuitBuilder& builder,
                                              const std::vector<std::vector<WireId>>& mask_coefficients,
                                              const std::vector<WireId>& hsk_bits,
                                              const std::string& prefix);
};

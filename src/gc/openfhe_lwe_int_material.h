#pragma once

#include "src/gc/openfhe_lwe_int_decrypt_compare_circuit.h"

#include <cstdint>
#include <string>
#include <vector>

struct OpenFHELWEIntCiphertextMaterial {
    uint64_t plaintext = 0;
    size_t dimension = 0;
    uint64_t ciphertext_modulus = 0;
    uint64_t plaintext_modulus = 0;
    uint64_t body = 0;
    std::vector<uint64_t> a;
    uint64_t manual_dec = 0;
};

struct OpenFHELWEIntMaterial {
    size_t logical_plaintext_bits = 0;
    uint64_t plaintext_modulus = 0;
    size_t hsk_dimension = 0;
    uint64_t hsk_modulus = 0;
    uint64_t hsk_switched_modulus = 0;
    std::vector<uint64_t> hsk_mod_q;
    OpenFHELWEIntCiphertextMaterial a_prime;
    OpenFHELWEIntCiphertextMaterial b_prime;
    OpenFHELWEIntCiphertextMaterial one_prime;

    OpenFHELWEIntCircuitParams CircuitParams() const;
};

OpenFHELWEIntMaterial ReadOpenFHELWEIntMaterial(const std::string& path);

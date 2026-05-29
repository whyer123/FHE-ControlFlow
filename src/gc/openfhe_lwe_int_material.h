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

struct OpenFHELWEIntRuntimeCiphertextMaterial {
    size_t dimension = 0;
    uint64_t ciphertext_modulus = 0;
    uint64_t plaintext_modulus = 0;
    uint64_t body = 0;
    std::vector<uint64_t> a;
};

struct OpenFHELWEIntRuntimeMaterial {
    size_t logical_plaintext_bits = 0;
    uint64_t plaintext_modulus = 0;
    OpenFHELWEIntRuntimeCiphertextMaterial a_prime;
    OpenFHELWEIntRuntimeCiphertextMaterial b_prime;
    OpenFHELWEIntRuntimeCiphertextMaterial one_prime;

    size_t ModulusBits() const;
};

OpenFHELWEIntMaterial ReadOpenFHELWEIntMaterial(const std::string& path);
OpenFHELWEIntRuntimeMaterial ToRuntimeMaterial(
    const OpenFHELWEIntMaterial& material);
void WriteOpenFHELWEIntRuntimeMaterial(
    const OpenFHELWEIntRuntimeMaterial& material,
    const std::string& path);
OpenFHELWEIntRuntimeMaterial ReadOpenFHELWEIntRuntimeMaterial(
    const std::string& path);

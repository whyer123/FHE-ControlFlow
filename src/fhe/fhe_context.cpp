#include "fhe_context.h"
#include <iostream>

FHEContextWrapper::FHEContextWrapper() {
    // Generate context
    cc.GenerateBinFHEContext(TOY); // Using TOY for fast prototyping, change to STD128 for security

    // Generate keys
    sk = cc.KeyGen();

    std::cout << "Creating bootstrapping keys, this might take a moment..." << std::endl;
    // Generate bootstrapping keys
    cc.BTKeyGen(sk);
    std::cout << "Bootstrapping keys generated." << std::endl;
}

std::vector<LWECiphertext> FHEContextWrapper::EncryptInteger(int64_t value, size_t bit_length) {
    std::vector<LWECiphertext> result;
    for (size_t i = 0; i < bit_length; ++i) {
        int bit = (value >> i) & 1;
        result.push_back(cc.Encrypt(sk, bit));
    }
    return result;
}

int64_t FHEContextWrapper::DecryptInteger(const std::vector<LWECiphertext>& cipher_bits) {
    int64_t result = 0;
    for (size_t i = 0; i < cipher_bits.size(); ++i) {
        LWEPlaintext plain_bit;
        cc.Decrypt(sk, cipher_bits[i], &plain_bit);
        if (plain_bit == 1) {
            result |= (1ULL << i);
        }
    }
    return result;
}

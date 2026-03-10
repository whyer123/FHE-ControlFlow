#pragma once

#include "src/fhe/fhe_context.h"

// Interface for extracting a condition bit
class SelectorAPI {
public:
    virtual ~SelectorAPI() = default;

    // Given a ciphertext bit representing a condition, extract it as a plaintext boolean.
    // In a real system, this would involve a cryptographic protocol (like GC).
    virtual bool ExtractConditionBit(const LWECiphertext& cond_bit) = 0;
};

// A trusted selector that simply decrypts the result (for prototyping)
class TrustedSelector : public SelectorAPI {
public:
    TrustedSelector(FHEContextWrapper& fhe) : fhe_ctx(fhe) {}

    bool ExtractConditionBit(const LWECiphertext& cond_bit) override;

private:
    FHEContextWrapper& fhe_ctx;
};

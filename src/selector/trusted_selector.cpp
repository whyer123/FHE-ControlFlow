#include "selector_api.h"

bool TrustedSelector::ExtractConditionBit(const LWECiphertext& cond_bit) {
    // For the prototype, we assume the trusted selector has access to the decryption key.
    // We decrypt the single condition bit to determine if the loop should continue.
    LWEPlaintext plain_bit;
    fhe_ctx.GetContext().Decrypt(fhe_ctx.GetSecretKey(), cond_bit, &plain_bit);
    return plain_bit == 1;
}

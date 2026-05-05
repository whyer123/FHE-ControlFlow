#include "selector_api.h"

bool TrustedSelector::ExtractBranch(const LWECiphertext& cond_bit) {
    LWEPlaintext plain_bit;
    fhe_ctx.GetContext().Decrypt(fhe_ctx.GetSecretKey(), cond_bit, &plain_bit);
    return plain_bit == 1;
}

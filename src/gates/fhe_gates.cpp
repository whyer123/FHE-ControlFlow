#include "fhe_gates.h"

LWECiphertext FHEGates::EvalAND(const LWECiphertext& ct1, const LWECiphertext& ct2) {
    return fhe_ctx.GetContext().EvalBinGate(lbcrypto::AND, ct1, ct2);
}

LWECiphertext FHEGates::EvalXNOR(const LWECiphertext& ct1, const LWECiphertext& ct2) {
    return fhe_ctx.GetContext().EvalBinGate(lbcrypto::XNOR, ct1, ct2);
}

LWECiphertext FHEGates::EvalXOR(const LWECiphertext& ct1, const LWECiphertext& ct2) {
    return fhe_ctx.GetContext().EvalBinGate(lbcrypto::XOR, ct1, ct2);
}

LWECiphertext FHEGates::EvalOR(const LWECiphertext& ct1, const LWECiphertext& ct2) {
    return fhe_ctx.GetContext().EvalBinGate(lbcrypto::OR, ct1, ct2);
}

LWECiphertext FHEGates::EvalNOT(const LWECiphertext& ct) {
    return fhe_ctx.GetContext().EvalNOT(ct);
}

LWECiphertext FHEGates::EvalMUX(const LWECiphertext& sel, const LWECiphertext& ct1, const LWECiphertext& ct2) {
    // MUX = (sel AND ct1) OR (NOT(sel) AND ct2)
    // Actually XOR is often used instead of OR since the terms are mutually exclusive
    // MUX = (sel AND ct1) XOR (NOT(sel) AND ct2)
    auto not_sel = EvalNOT(sel);
    auto t1 = EvalAND(sel, ct1);
    auto t2 = EvalAND(not_sel, ct2);
    return fhe_ctx.GetContext().EvalBinGate(lbcrypto::XOR, t1, t2);
}

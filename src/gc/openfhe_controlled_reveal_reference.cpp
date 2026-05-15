// Boolean-circuit lowering source for:
//
//     Dec_hsk(OpenFHE.Eval([x <= b], x', b'))
//
// This file intentionally does not include OpenFHE or any project headers.
// Everything below is written as local structs, constants, and arithmetic so it
// can be lowered into Boolean gates one function at a time.

namespace controlled_reveal_reference {

typedef unsigned int u32;
typedef long long i64;

enum GateKind {
    GateAnd,
    GateXor
};

// OpenFHE BinFHE STD128-shaped small LWE parameters used by the lowering
// source. The application integer width stays 4 bits because the current loop
// prototype compares encrypted 4-bit a' and b'.
static const u32 kLweDimension = 556;
static const u32 kCiphertextModulus = 2048;
static const u32 kPlaintextModulus = 4;
static const u32 kEncodedOne = kCiphertextModulus / kPlaintextModulus;
static const u32 kRoundingOffset =
    kCiphertextModulus / (2 * kPlaintextModulus);
static const u32 kIntegerBits = 4;

struct LweCiphertext {
    u32 a[kLweDimension];
    u32 b;
};

struct IntegerCiphertext {
    LweCiphertext bits[kIntegerBits];
};

// Fixed hsk for the future fixed-GC demo.
//
// In the GC setup phase these coefficients become fixed secret constants. The
// evaluator receives only selected labels for these constants, not the numeric
// hsk values. Coefficients are ternary as in OpenFHE's UNIFORM_TERNARY secret
// distribution. The tail is explicitly zero-filled by C++ initialization.
static const signed char FIXED_HSK[kLweDimension] = {
     1,  0, -1,  1, -1,  0,  1,  1, -1,  0,  0,  1, -1,  1,  0, -1,
     1, -1,  0,  1,  0, -1, -1,  1,  1,  0, -1,  0,  1, -1,  1,  0,
    -1,  1,  0, -1,  1,  1,  0, -1,  0,  1, -1,  0,  1,  1, -1,  0,
     0, -1,  1,  0,  1, -1,  0,  1, -1, -1,  1,  0,  1,  0, -1,  1,
    -1,  0,  1,  1,  0, -1,  1,  0, -1,  1, -1,  0,  1,  0, -1, -1,
     1,  1,  0, -1,  0,  1, -1,  1,  0,  0, -1,  1,  1, -1,  0,  1,
     0, -1,  1, -1,  0,  1,  1,  0, -1, -1,  1,  0,  1, -1,  0,  1,
    -1,  1,  0,  0,  1, -1,  1,  0, -1,  1,  0, -1,  1, -1,  0,  1
};

u32 ModQ(i64 value) {
    i64 reduced = value % static_cast<i64>(kCiphertextModulus);
    if (reduced < 0) {
        reduced += kCiphertextModulus;
    }
    return static_cast<u32>(reduced);
}

u32 AddModQ(u32 lhs, u32 rhs) {
    return ModQ(static_cast<i64>(lhs) + static_cast<i64>(rhs));
}

u32 SubModQ(u32 lhs, u32 rhs) {
    return ModQ(static_cast<i64>(lhs) - static_cast<i64>(rhs));
}

u32 MulBySecretCoeffModQ(u32 value, signed char secret_coeff) {
    if (secret_coeff == 0) {
        return 0;
    }
    if (secret_coeff > 0) {
        return value % kCiphertextModulus;
    }
    return value == 0 ? 0 : kCiphertextModulus - value;
}

LweCiphertext ZeroCiphertext() {
    LweCiphertext out = {};
    out.b = 0;
    return out;
}

LweCiphertext AddCiphertexts(const LweCiphertext& lhs,
                             const LweCiphertext& rhs) {
    LweCiphertext out = {};
    for (u32 i = 0; i < kLweDimension; ++i) {
        out.a[i] = AddModQ(lhs.a[i], rhs.a[i]);
    }
    out.b = AddModQ(lhs.b, rhs.b);
    return out;
}

LweCiphertext DoubleCiphertext(const LweCiphertext& value) {
    return AddCiphertexts(value, value);
}

LweCiphertext EvalNot(const LweCiphertext& value) {
    LweCiphertext out = {};
    for (u32 i = 0; i < kLweDimension; ++i) {
        out.a[i] = value.a[i] == 0 ? 0 : kCiphertextModulus - value.a[i];
    }
    out.b = SubModQ(kEncodedOne, value.b);
    return out;
}

u32 DotWithFixedHsk(const LweCiphertext& value) {
    u32 total = 0;
    for (u32 i = 0; i < kLweDimension; ++i) {
        total = AddModQ(total, MulBySecretCoeffModQ(value.a[i], FIXED_HSK[i]));
    }
    return total;
}

u32 PhaseWithFixedHsk(const LweCiphertext& value) {
    return SubModQ(value.b, DotWithFixedHsk(value));
}

bool DecFixedHsk(const LweCiphertext& value) {
    // This is the Dec_hsk arithmetic that will become Boolean gates:
    //
    //     phase = b - <a,hsk> mod q
    //     decoded = floor(p * (phase + q/(2p)) / q)
    const u32 phase = PhaseWithFixedHsk(value);
    const u32 rounded = AddModQ(phase, kRoundingOffset);
    const u32 decoded = (kPlaintextModulus * rounded) / kCiphertextModulus;
    return (decoded & 1U) != 0;
}

u32 FixedMaskCoeff(u32 index, u32 domain) {
    // Deterministic public mask for this lowering source. It is not part of
    // the final OpenFHE security story; it exists so the source can build a
    // valid LWE-shaped ciphertext after the bootstrap placeholder.
    return (37U * index + 131U * domain + 17U) % kCiphertextModulus;
}

LweCiphertext EncryptBitWithFixedMask(bool bit, u32 domain) {
    LweCiphertext out = {};
    for (u32 i = 0; i < kLweDimension; ++i) {
        out.a[i] = FixedMaskCoeff(i, domain);
    }

    const u32 message = bit ? kEncodedOne : 0;
    out.b = AddModQ(DotWithFixedHsk(out), message);
    return out;
}

bool DecodeOpenFHEGateInputPhase(GateKind gate, u32 phase) {
    // OpenFHE EvalBinGate first forms an additive LWE ciphertext:
    //   AND-family gates: ct1 + ct2
    //   XOR-family gates: 2 * (ct1 + ct2)
    //
    // Full OpenFHE then bootstraps this phase through a gate-specific LUT.
    // This function is the isolated placeholder for that LUT while we are
    // preparing the source for Boolean-circuit lowering.
    if (gate == GateAnd) {
        return phase >= (2U * kEncodedOne);
    }
    return phase == (2U * kEncodedOne);
}

LweCiphertext BootstrapGateCoreForLowering(GateKind gate,
                                           const LweCiphertext& prebootstrap,
                                           u32 domain) {
    // TODO for the next milestone:
    // Replace this semantic LUT with the real OpenFHE BootstrapGateCore:
    // blind rotation, RGSW accumulator processing, extraction, and key switch.
    const bool bootstrapped_bit =
        DecodeOpenFHEGateInputPhase(gate, PhaseWithFixedHsk(prebootstrap));
    return EncryptBitWithFixedMask(bootstrapped_bit, domain);
}

LweCiphertext EvalBinGate(GateKind gate,
                          const LweCiphertext& lhs,
                          const LweCiphertext& rhs,
                          u32 domain) {
    LweCiphertext prebootstrap = AddCiphertexts(lhs, rhs);
    if (gate == GateXor) {
        prebootstrap = DoubleCiphertext(prebootstrap);
    }
    return BootstrapGateCoreForLowering(gate, prebootstrap, domain);
}

LweCiphertext EvalAnd(const LweCiphertext& lhs,
                      const LweCiphertext& rhs,
                      u32 domain) {
    return EvalBinGate(GateAnd, lhs, rhs, domain);
}

LweCiphertext EvalXor(const LweCiphertext& lhs,
                      const LweCiphertext& rhs,
                      u32 domain) {
    return EvalBinGate(GateXor, lhs, rhs, domain);
}

LweCiphertext EvalLessOrEqualPredicate(const IntegerCiphertext& x,
                                       const IntegerCiphertext& bound) {
    // This function is:
    //
    //     OpenFHE.Eval([x <= b], x', b')
    //
    // x.bits[i] and bound.bits[i] are LWE ciphertexts for plaintext bits.
    LweCiphertext greater = EvalAnd(EvalNot(bound.bits[0]), x.bits[0], 1);

    for (u32 i = 1; i < kIntegerBits; ++i) {
        const u32 domain = 10U * i;
        const LweCiphertext generate =
            EvalAnd(EvalNot(bound.bits[i]), x.bits[i], domain + 1U);
        const LweCiphertext xor_bits =
            EvalXor(bound.bits[i], x.bits[i], domain + 2U);
        const LweCiphertext equal_bits = EvalNot(xor_bits);
        const LweCiphertext propagate =
            EvalAnd(equal_bits, greater, domain + 3U);

        // generate and propagate are mutually exclusive in this comparator.
        greater = EvalXor(generate, propagate, domain + 4U);
    }

    return EvalNot(greater);
}

bool ControlledReveal(const IntegerCiphertext& x,
                      const IntegerCiphertext& bound) {
    // This is the exact target shape:
    //
    //     Dec_hsk(OpenFHE.Eval([x <= b], x', b'))
    //
    // hsk is fixed in FIXED_HSK above. There is no hsk function parameter and
    // no path-based key loading in this lowering source.
    const LweCiphertext predicate_ciphertext =
        EvalLessOrEqualPredicate(x, bound);
    return DecFixedHsk(predicate_ciphertext);
}

} // namespace controlled_reveal_reference

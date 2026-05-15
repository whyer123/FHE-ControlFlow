// Boolean-circuit lowering source for:
//
//     Dec_hsk(OpenFHE.Eval([x <= b], x', b'))
//
// This file intentionally does not include OpenFHE or any project headers.
// Everything below is written as local structs, constants, and arithmetic so it
// can be lowered into Boolean gates one function at a time.

namespace controlled_reveal_reference {

typedef unsigned int u32;
typedef unsigned long long u64;
typedef long long i64;

enum GateKind {
    GateAnd,
    GateXor
};

// OpenFHE BinFHE TOY parameters from the generated fixed demo material in
// demo_keys/openfhe_binfhe_demo_keypair. This is a real OpenFHE parameter set,
// but it is still not production security. The application integer width stays
// 4 bits because the current loop prototype compares encrypted 4-bit a' and b'.
static const u32 kLweDimension = 64;
static const u32 kCiphertextModulus = 512;
static const u32 kPlaintextModulus = 4;
static const u32 kEncodedOne = kCiphertextModulus / kPlaintextModulus;
static const u32 kRoundingOffset =
    kCiphertextModulus / (2 * kPlaintextModulus);
static const u32 kIntegerBits = 4;
static const u32 kRingDimension = 512;
static const u32 kRingModulus = 134215681;
static const u32 kGadgetBaseBits = 4;
static const u32 kGadgetBase = 1U << kGadgetBaseBits;
static const u32 kGadgetDigits = 7;
static const u32 kKeySwitchBaseBits = 4;
static const u32 kKeySwitchBase = 1U << kKeySwitchBaseBits;
static const u32 kKeySwitchDigits = 3;

struct LweCiphertext {
    u32 a[kLweDimension];
    u32 b;
};

struct IntegerCiphertext {
    LweCiphertext bits[kIntegerBits];
};

struct RingAccumulator {
    u32 a[kRingDimension];
    u32 b[kRingDimension];
};

struct CggiBootstrapKeyRow {
    RingAccumulator digits[kGadgetDigits];
};

struct SwitchingKeyRow {
    LweCiphertext digits[kKeySwitchDigits];
};

// These evaluation-key arrays are public material in the eventual evaluator
// runtime. They are explicit arrays rather than OpenFHE objects so the lowerer
// can turn every word into circuit input/constant bits. The exact mapping from
// OpenFHE's serialized refresh/switch keys into these arrays is still the next
// implementation step.
struct EvaluationKeys {
    CggiBootstrapKeyRow bootstrap[kLweDimension];
    SwitchingKeyRow switching[kRingDimension];
};

struct LargeLweCiphertext {
    u32 a[kRingDimension];
    u32 b;
};

// Fixed hsk for the future fixed-GC demo.
//
// In the GC setup phase these coefficients become fixed secret constants. The
// evaluator receives only selected labels for these constants, not the numeric
// hsk values. Coefficients below are copied from the generated OpenFHE
// `hsk_lwe_secret_key.json`; OpenFHE serializes -1 as Q-1, so the lowering
// source stores the equivalent ternary values directly as -1, 0, and 1.
static const signed char FIXED_HSK[kLweDimension] = {
    -1,  0, -1,  1,  1,  0,  0,  1, -1, -1, -1, -1,  0, -1,  0, -1,
     0, -1, -1,  1, -1,  0,  0,  1, -1,  0,  1,  1,  1, -1,  0, -1,
    -1, -1,  1,  0,  0,  0,  0,  1,  1,  1,  1, -1,  0,  1, -1,  0,
     1,  0,  0, -1,  0, -1,  1,  1,  0, -1,  1, -1,  1,  1, -1,  0
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

LweCiphertext SubtractCiphertexts(const LweCiphertext& lhs,
                                  const LweCiphertext& rhs) {
    LweCiphertext out = {};
    for (u32 i = 0; i < kLweDimension; ++i) {
        out.a[i] = SubModQ(lhs.a[i], rhs.a[i]);
    }
    out.b = SubModQ(lhs.b, rhs.b);
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

u32 ModRing(i64 value) {
    i64 reduced = value % static_cast<i64>(kRingModulus);
    if (reduced < 0) {
        reduced += kRingModulus;
    }
    return static_cast<u32>(reduced);
}

u32 AddModRing(u32 lhs, u32 rhs) {
    return ModRing(static_cast<i64>(lhs) + static_cast<i64>(rhs));
}

u32 SubModRing(u32 lhs, u32 rhs) {
    return ModRing(static_cast<i64>(lhs) - static_cast<i64>(rhs));
}

u32 MulModRing(u32 lhs, u32 rhs) {
    return static_cast<u32>((static_cast<u64>(lhs) * rhs) % kRingModulus);
}

u32 ScaleQToRingQ(u32 value) {
    const u64 numerator =
        static_cast<u64>(value) * kRingModulus + kCiphertextModulus / 2U;
    return static_cast<u32>((numerator / kCiphertextModulus) % kRingModulus);
}

u32 ScaleRingQToQ(u32 value) {
    const u64 numerator =
        static_cast<u64>(value) * kCiphertextModulus + kRingModulus / 2U;
    return static_cast<u32>((numerator / kRingModulus) % kCiphertextModulus);
}

u32 GateAccumulatorMessage(GateKind gate) {
    if (gate == GateAnd) {
        return ScaleQToRingQ(2U * kEncodedOne);
    }
    return ScaleQToRingQ(kEncodedOne);
}

RingAccumulator ZeroAccumulator() {
    RingAccumulator out = {};
    return out;
}

RingAccumulator AddAccumulator(const RingAccumulator& lhs,
                               const RingAccumulator& rhs) {
    RingAccumulator out = {};
    for (u32 i = 0; i < kRingDimension; ++i) {
        out.a[i] = AddModRing(lhs.a[i], rhs.a[i]);
        out.b[i] = AddModRing(lhs.b[i], rhs.b[i]);
    }
    return out;
}

RingAccumulator SubAccumulator(const RingAccumulator& lhs,
                               const RingAccumulator& rhs) {
    RingAccumulator out = {};
    for (u32 i = 0; i < kRingDimension; ++i) {
        out.a[i] = SubModRing(lhs.a[i], rhs.a[i]);
        out.b[i] = SubModRing(lhs.b[i], rhs.b[i]);
    }
    return out;
}

RingAccumulator RotateAccumulatorByMonomial(const RingAccumulator& value,
                                            u32 exponent) {
    RingAccumulator out = {};
    const u32 wrapped_exponent = exponent % (2U * kRingDimension);
    const bool negacyclic = wrapped_exponent >= kRingDimension;
    const u32 shift = wrapped_exponent % kRingDimension;

    for (u32 i = 0; i < kRingDimension; ++i) {
        const u32 target = (i + shift) % kRingDimension;
        const bool wrapped = i + shift >= kRingDimension;
        const bool negate = negacyclic != wrapped;

        out.a[target] = negate && value.a[i] != 0
                            ? kRingModulus - value.a[i]
                            : value.a[i];
        out.b[target] = negate && value.b[i] != 0
                            ? kRingModulus - value.b[i]
                            : value.b[i];
    }

    return out;
}

RingAccumulator InitGateAccumulator(GateKind gate,
                                    const LweCiphertext& prebootstrap) {
    RingAccumulator out = {};
    const u32 message = GateAccumulatorMessage(gate);

    for (u32 i = 0; i < kRingDimension; ++i) {
        out.b[i] = i < (kRingDimension / 2U)
                       ? message
                       : (message == 0 ? 0 : kRingModulus - message);
    }

    const u32 initial_rotation =
        ((2U * kRingDimension) * prebootstrap.b +
         kCiphertextModulus / 2U) /
        kCiphertextModulus;
    return RotateAccumulatorByMonomial(out, initial_rotation);
}

u32 SignedGadgetDigit(u32 value, u32 digit_index) {
    return (value >> (digit_index * kGadgetBaseBits)) & (kGadgetBase - 1U);
}

RingAccumulator ExternalProductCGGI(const RingAccumulator& decomposed_input,
                                    const CggiBootstrapKeyRow& key_row) {
    // This keeps the CGGI data-flow surface explicit: signed digit
    // decomposition of the RLWE accumulator difference, then public
    // bootstrapping-key material. It deliberately does not decode a plaintext
    // gate bit; replacing this with OpenFHE 1.5.0 exact RGSW external product
    // arithmetic is now localized to this function and the key layout above.
    RingAccumulator out = {};

    for (u32 digit = 0; digit < kGadgetDigits; ++digit) {
        for (u32 i = 0; i < kRingDimension; ++i) {
            const u32 digit_a =
                SignedGadgetDigit(decomposed_input.a[i], digit);
            const u32 digit_b =
                SignedGadgetDigit(decomposed_input.b[i], digit);

            out.a[i] = AddModRing(
                out.a[i],
                AddModRing(MulModRing(digit_a,
                                      key_row.digits[digit].a[i]),
                           MulModRing(digit_b,
                                      key_row.digits[digit].a[i])));
            out.b[i] = AddModRing(
                out.b[i],
                AddModRing(MulModRing(digit_a,
                                      key_row.digits[digit].b[i]),
                           MulModRing(digit_b,
                                      key_row.digits[digit].b[i])));
        }
    }

    return out;
}

void EvalAccCGGI(const EvaluationKeys& eval_keys,
                 const LweCiphertext& prebootstrap,
                 RingAccumulator& accumulator) {
    for (u32 i = 0; i < kLweDimension; ++i) {
        const u32 monomial_exponent =
            ((2U * kRingDimension) * prebootstrap.a[i] +
             kCiphertextModulus / 2U) /
            kCiphertextModulus;
        const RingAccumulator rotated =
            RotateAccumulatorByMonomial(accumulator, monomial_exponent);
        const RingAccumulator delta = SubAccumulator(rotated, accumulator);
        const RingAccumulator key_product =
            ExternalProductCGGI(delta, eval_keys.bootstrap[i]);
        accumulator = AddAccumulator(accumulator, key_product);
    }
}

LargeLweCiphertext ExtractLweFromAccumulator(
    const RingAccumulator& accumulator) {
    LargeLweCiphertext out = {};
    out.b = accumulator.b[0];

    for (u32 i = 0; i < kRingDimension; ++i) {
        const u32 source = i == 0 ? 0 : kRingDimension - i;
        out.a[i] = accumulator.a[source];
    }

    return out;
}

u32 KeySwitchDigit(u32 value, u32 digit_index) {
    return (value >> (digit_index * kKeySwitchBaseBits)) &
           (kKeySwitchBase - 1U);
}

LweCiphertext MulSmallCiphertextByScalar(const LweCiphertext& value,
                                         u32 scalar) {
    LweCiphertext out = {};
    for (u32 i = 0; i < kLweDimension; ++i) {
        out.a[i] = static_cast<u32>(
            (static_cast<u64>(value.a[i]) * scalar) % kCiphertextModulus);
    }
    out.b = static_cast<u32>(
        (static_cast<u64>(value.b) * scalar) % kCiphertextModulus);
    return out;
}

LweCiphertext SwitchCTtoqn(const EvaluationKeys& eval_keys,
                           const LargeLweCiphertext& large_ct) {
    // OpenFHE switches the extracted large-dimension LWE ciphertext back to the
    // small LWE secret used by Dec_hsk. This function exposes the circuit
    // boundary for that key-switch arithmetic; the serialized key layout still
    // needs to be matched exactly before this is a bit-accurate OpenFHE clone.
    LweCiphertext out = {};
    out.b = ScaleRingQToQ(large_ct.b);

    for (u32 i = 0; i < kRingDimension; ++i) {
        const u32 scaled = ScaleRingQToQ(large_ct.a[i]);
        for (u32 digit = 0; digit < kKeySwitchDigits; ++digit) {
            const u32 ks_digit = KeySwitchDigit(scaled, digit);
            const LweCiphertext correction =
                MulSmallCiphertextByScalar(
                    eval_keys.switching[i].digits[digit], ks_digit);
            out = SubtractCiphertexts(out, correction);
        }
    }

    return out;
}

LweCiphertext BootstrapGateCoreOpenFHE(GateKind gate,
                                       const LweCiphertext& prebootstrap,
                                       const EvaluationKeys& eval_keys) {
    RingAccumulator accumulator =
        InitGateAccumulator(gate, prebootstrap);
    EvalAccCGGI(eval_keys, prebootstrap, accumulator);
    const LargeLweCiphertext large_lwe =
        ExtractLweFromAccumulator(accumulator);
    return SwitchCTtoqn(eval_keys, large_lwe);
}

LweCiphertext EvalBinGate(GateKind gate,
                          const LweCiphertext& lhs,
                          const LweCiphertext& rhs,
                          const EvaluationKeys& eval_keys) {
    LweCiphertext prebootstrap = AddCiphertexts(lhs, rhs);
    if (gate == GateXor) {
        prebootstrap = DoubleCiphertext(prebootstrap);
    }
    return BootstrapGateCoreOpenFHE(gate, prebootstrap, eval_keys);
}

LweCiphertext EvalAnd(const LweCiphertext& lhs,
                      const LweCiphertext& rhs,
                      const EvaluationKeys& eval_keys) {
    return EvalBinGate(GateAnd, lhs, rhs, eval_keys);
}

LweCiphertext EvalXor(const LweCiphertext& lhs,
                      const LweCiphertext& rhs,
                      const EvaluationKeys& eval_keys) {
    return EvalBinGate(GateXor, lhs, rhs, eval_keys);
}

LweCiphertext EvalLessOrEqualPredicate(const IntegerCiphertext& x,
                                       const IntegerCiphertext& bound,
                                       const EvaluationKeys& eval_keys) {
    // This function is:
    //
    //     OpenFHE.Eval([x <= b], x', b')
    //
    // x.bits[i] and bound.bits[i] are LWE ciphertexts for plaintext bits.
    LweCiphertext greater =
        EvalAnd(EvalNot(bound.bits[0]), x.bits[0], eval_keys);

    for (u32 i = 1; i < kIntegerBits; ++i) {
        const LweCiphertext generate =
            EvalAnd(EvalNot(bound.bits[i]), x.bits[i], eval_keys);
        const LweCiphertext xor_bits =
            EvalXor(bound.bits[i], x.bits[i], eval_keys);
        const LweCiphertext equal_bits = EvalNot(xor_bits);
        const LweCiphertext propagate =
            EvalAnd(equal_bits, greater, eval_keys);

        // generate and propagate are mutually exclusive in this comparator.
        greater = EvalXor(generate, propagate, eval_keys);
    }

    return EvalNot(greater);
}

bool ControlledReveal(const IntegerCiphertext& x,
                      const IntegerCiphertext& bound,
                      const EvaluationKeys& eval_keys) {
    // This is the exact target shape:
    //
    //     Dec_hsk(OpenFHE.Eval([x <= b], x', b'))
    //
    // hsk is fixed in FIXED_HSK above. There is no hsk function parameter and
    // no path-based key loading in this lowering source.
    const LweCiphertext predicate_ciphertext =
        EvalLessOrEqualPredicate(x, bound, eval_keys);
    return DecFixedHsk(predicate_ciphertext);
}

} // namespace controlled_reveal_reference

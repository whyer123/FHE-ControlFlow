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
static const u32 kGadgetBaseBits = 9;
static const u32 kGadgetBase = 512;
static const u32 kGadgetDigits = 3;
static const u32 kCggiExternalProductDigits = (kGadgetDigits - 1U) * 2U;
static const u32 kKeySwitchBase = 25;
static const u32 kKeySwitchDigits = 6;
static const u32 kAccumulatorPositiveMessage =
    kRingModulus / (kPlaintextModulus * 2U) + 1U;
static const u32 kAccumulatorNegativeMessage =
    kRingModulus - kAccumulatorPositiveMessage;

struct LweCiphertext {
    u32 a[kLweDimension];
    u32 b;
};

struct IntegerCiphertext {
    LweCiphertext bits[kIntegerBits];
};

struct RingPolynomial {
    u32 coeffs[kRingDimension];
};

struct RingAccumulator {
    RingPolynomial a;
    RingPolynomial b;
};

struct CggiBootstrapKeyRow {
    RingAccumulator digits[kCggiExternalProductDigits];
};

struct DecomposedAccumulator {
    RingPolynomial digits[kCggiExternalProductDigits];
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
    bool material_loaded;
    CggiBootstrapKeyRow bootstrap[2][kLweDimension];
    SwitchingKeyRow switching[kRingDimension];
};

struct LargeLweCiphertext {
    u32 a[kRingDimension];
    u32 b;
};

struct GateRange {
    u32 q1;
    u32 q2;
    u32 lb;
    u32 ub;
    bool swap;
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

u32 ScaleRingQToQ(u32 value) {
    const u64 numerator =
        static_cast<u64>(value) * kCiphertextModulus + kRingModulus / 2U;
    return static_cast<u32>((numerator / kRingModulus) % kCiphertextModulus);
}

u32 GateConstant(GateKind gate) {
    // OpenFHE RingGSWCryptoParams::PreCompute stores:
    //   AND = 7 * (q >> 3)
    //   XOR = 6 * (q >> 3)
    if (gate == GateAnd) {
        return 7U * (kCiphertextModulus >> 3);
    }
    return 6U * (kCiphertextModulus >> 3);
}

GateRange GateAccumulatorRange(GateKind gate) {
    const u32 q1 = GateConstant(gate);
    const u32 q2 = AddModQ(q1, kCiphertextModulus >> 1);
    const bool swap = q1 >= q2;

    GateRange out = {};
    out.q1 = q1;
    out.q2 = q2;
    out.lb = swap ? q2 : q1;
    out.ub = swap ? q1 : q2;
    out.swap = swap;
    return out;
}

bool InHalfOpenRange(u32 value, u32 lb, u32 ub) {
    return value >= lb && value < ub;
}

RingAccumulator ZeroAccumulator() {
    RingAccumulator out = {};
    return out;
}

RingAccumulator AddAccumulator(const RingAccumulator& lhs,
                               const RingAccumulator& rhs) {
    RingAccumulator out = {};
    for (u32 i = 0; i < kRingDimension; ++i) {
        out.a.coeffs[i] = AddModRing(lhs.a.coeffs[i], rhs.a.coeffs[i]);
        out.b.coeffs[i] = AddModRing(lhs.b.coeffs[i], rhs.b.coeffs[i]);
    }
    return out;
}

RingAccumulator SubAccumulator(const RingAccumulator& lhs,
                               const RingAccumulator& rhs) {
    RingAccumulator out = {};
    for (u32 i = 0; i < kRingDimension; ++i) {
        out.a.coeffs[i] = SubModRing(lhs.a.coeffs[i], rhs.a.coeffs[i]);
        out.b.coeffs[i] = SubModRing(lhs.b.coeffs[i], rhs.b.coeffs[i]);
    }
    return out;
}

RingPolynomial AddPolynomials(const RingPolynomial& lhs,
                              const RingPolynomial& rhs) {
    RingPolynomial out = {};
    for (u32 i = 0; i < kRingDimension; ++i) {
        out.coeffs[i] = AddModRing(lhs.coeffs[i], rhs.coeffs[i]);
    }
    return out;
}

RingPolynomial MultiplyByNegacyclicMonomial(const RingPolynomial& value,
                                            u32 exponent) {
    RingPolynomial out = {};
    const u32 wrapped_exponent = exponent % (2U * kRingDimension);
    const bool negacyclic = wrapped_exponent >= kRingDimension;
    const u32 shift = wrapped_exponent % kRingDimension;

    for (u32 i = 0; i < kRingDimension; ++i) {
        const u32 target = (i + shift) % kRingDimension;
        const bool wrapped = i + shift >= kRingDimension;
        const bool negate = negacyclic != wrapped;

        out.coeffs[target] = negate && value.coeffs[i] != 0
                                 ? kRingModulus - value.coeffs[i]
                                 : value.coeffs[i];
    }

    return out;
}

RingAccumulator MultiplyAccumulatorByNegacyclicMonomial(
    const RingAccumulator& value,
    u32 exponent) {
    RingAccumulator out = {};
    out.a = MultiplyByNegacyclicMonomial(value.a, exponent);
    out.b = MultiplyByNegacyclicMonomial(value.b, exponent);
    return out;
}

RingPolynomial MultiplyPolynomials(const RingPolynomial& lhs,
                                   const RingPolynomial& rhs) {
    RingPolynomial out = {};
    for (u32 i = 0; i < kRingDimension; ++i) {
        for (u32 j = 0; j < kRingDimension; ++j) {
            const u32 product = MulModRing(lhs.coeffs[i], rhs.coeffs[j]);
            const u32 index = i + j;
            if (index < kRingDimension) {
                out.coeffs[index] = AddModRing(out.coeffs[index], product);
            } else {
                out.coeffs[index - kRingDimension] =
                    SubModRing(out.coeffs[index - kRingDimension], product);
            }
        }
    }
    return out;
}

RingAccumulator InitGateAccumulator(GateKind gate,
                                    const LweCiphertext& prebootstrap) {
    RingAccumulator out = {};
    const GateRange range = GateAccumulatorRange(gate);
    const u32 lv = range.swap ? kAccumulatorPositiveMessage
                              : kAccumulatorNegativeMessage;
    const u32 uv = range.swap ? kAccumulatorNegativeMessage
                              : kAccumulatorPositiveMessage;
    const u32 q_half = kCiphertextModulus >> 1;
    const u32 factor = kRingDimension / q_half;
    u32 shifted_b = prebootstrap.b;

    // OpenFHE sparsely embeds Z_q into Z_Q[X]/(X^N+1). Only every `factor`
    // coefficient is assigned; all other coefficients stay zero. OpenFHE then
    // NTT-converts this polynomial before CGGI accumulation.
    for (u32 i = 0; i < kRingDimension; i += factor) {
        out.b.coeffs[i] =
            InHalfOpenRange(shifted_b, range.lb, range.ub) ? lv : uv;
        shifted_b = SubModQ(shifted_b, 1U);
    }

    return out;
}

i64 CenteredRingValue(u32 value) {
    if (value < (kRingModulus >> 1)) {
        return static_cast<i64>(value);
    }
    return static_cast<i64>(value) - static_cast<i64>(kRingModulus);
}

i64 SignedBaseRemainder(i64 value) {
    i64 remainder = value % static_cast<i64>(kGadgetBase);
    const i64 half_base = static_cast<i64>(kGadgetBase / 2U);
    if (remainder >= half_base) {
        remainder -= kGadgetBase;
    }
    if (remainder < -half_base) {
        remainder += kGadgetBase;
    }
    return remainder;
}

u32 SignedDigitToRing(i64 digit) {
    return digit < 0 ? ModRing(digit) : static_cast<u32>(digit);
}

void DecomposeScalarIntoDigits(u32 value,
                               DecomposedAccumulator& out,
                               u32 digit_offset,
                               u32 coeff_index) {
    i64 centered = CenteredRingValue(value);
    i64 remainder = SignedBaseRemainder(centered);
    centered = (centered - remainder) / static_cast<i64>(kGadgetBase);

    for (u32 digit = digit_offset; digit < kCggiExternalProductDigits;
         digit += 2U) {
        remainder = SignedBaseRemainder(centered);
        centered = (centered - remainder) / static_cast<i64>(kGadgetBase);
        out.digits[digit].coeffs[coeff_index] =
            AddModRing(out.digits[digit].coeffs[coeff_index],
                       SignedDigitToRing(remainder));
    }
}

RingPolynomial SignedDigitDecomposePolynomial(u32 value) {
    DecomposedAccumulator out = {};
    DecomposeScalarIntoDigits(value, out, 0, 0);
    RingPolynomial packed = {};
    for (u32 digit = 0; digit < kCggiExternalProductDigits; ++digit) {
        packed.coeffs[digit] = out.digits[digit].coeffs[0];
    }
    return packed;
}

DecomposedAccumulator SignedDigitDecomposeAccumulator(
    const RingAccumulator& input) {
    DecomposedAccumulator out = {};
    for (u32 i = 0; i < kRingDimension; ++i) {
        DecomposeScalarIntoDigits(input.a.coeffs[i], out, 0, i);
        DecomposeScalarIntoDigits(input.b.coeffs[i], out, 1, i);
    }
    return out;
}

RingAccumulator ExternalProductCGGI(const DecomposedAccumulator& decomposed,
                                    const CggiBootstrapKeyRow& key_row) {
    // This keeps the CGGI data-flow surface explicit: signed digit
    // decomposition of the RLWE accumulator difference, then public
    // bootstrapping-key material. It deliberately does not decode a plaintext
    // gate bit; replacing this with OpenFHE 1.5.0 exact RGSW external product
    // arithmetic is now localized to this function and the key layout above.
    RingAccumulator out = {};

    for (u32 digit = 0; digit < kCggiExternalProductDigits; ++digit) {
        out.a = AddPolynomials(
            out.a,
            MultiplyPolynomials(decomposed.digits[digit],
                                key_row.digits[digit].a));
        out.b = AddPolynomials(
            out.b,
            MultiplyPolynomials(decomposed.digits[digit],
                                key_row.digits[digit].b));
    }

    return out;
}

u32 ModSwitchToMonomialExponent(u32 value) {
    return ((2U * kRingDimension) * value + kCiphertextModulus / 2U) /
           kCiphertextModulus;
}

u32 NegateMod(u32 value, u32 modulus) {
    return value == 0 ? 0 : modulus - value;
}

void AddToAccCGGI(const CggiBootstrapKeyRow& positive_key,
                  const CggiBootstrapKeyRow& negative_key,
                  u32 monomial_exponent,
                  RingAccumulator& accumulator) {
    const DecomposedAccumulator decomposed =
        SignedDigitDecomposeAccumulator(accumulator);
    const u32 positive_index = monomial_exponent % (2U * kRingDimension);
    const u32 negative_index =
        NegateMod(positive_index, 2U * kRingDimension);

    const RingAccumulator positive_product =
        MultiplyAccumulatorByNegacyclicMonomial(
            ExternalProductCGGI(decomposed, positive_key), positive_index);
    const RingAccumulator negative_product =
        MultiplyAccumulatorByNegacyclicMonomial(
            ExternalProductCGGI(decomposed, negative_key), negative_index);

    accumulator = AddAccumulator(
        AddAccumulator(accumulator, positive_product), negative_product);
}

void EvalAccCGGI(const EvaluationKeys& eval_keys,
                 const LweCiphertext& prebootstrap,
                 RingAccumulator& accumulator) {
    if (!eval_keys.material_loaded) {
        return;
    }

    for (u32 i = 0; i < kLweDimension; ++i) {
        const u32 monomial_exponent =
            ModSwitchToMonomialExponent(
                NegateMod(prebootstrap.a[i], kCiphertextModulus));
        AddToAccCGGI(eval_keys.bootstrap[0][i],
                     eval_keys.bootstrap[1][i],
                     monomial_exponent,
                     accumulator);
    }
}

LargeLweCiphertext ExtractLweFromAccumulator(
    const RingAccumulator& accumulator) {
    LargeLweCiphertext out = {};
    out.b = accumulator.b.coeffs[0];

    for (u32 i = 0; i < kRingDimension; ++i) {
        const u32 source = i == 0 ? 0 : kRingDimension - i;
        out.a[i] = accumulator.a.coeffs[source];
    }

    return out;
}

u32 KeySwitchDigit(u32 value, u32 digit_index) {
    u32 shifted = value;
    for (u32 i = 0; i < digit_index; ++i) {
        shifted /= kKeySwitchBase;
    }
    return shifted % kKeySwitchBase;
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

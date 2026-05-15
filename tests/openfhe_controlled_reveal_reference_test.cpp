#include "src/gc/openfhe_controlled_reveal_reference.cpp"

#include <cstdlib>

namespace {

using controlled_reveal_reference::EvaluationKeys;
using controlled_reveal_reference::IntegerCiphertext;
using controlled_reveal_reference::LweCiphertext;
using controlled_reveal_reference::RingPolynomial;
using controlled_reveal_reference::RingAccumulator;

LweCiphertext OnePrimeCiphertextFromOpenFHE() {
    return {{
        213, 383, 389,  83, 323, 344, 270, 283,
        391,  66, 505, 223,  57,  73, 138, 124,
        373, 126,  88,   6, 273, 160, 229, 384,
        124, 339, 511,  76, 402, 415,  99, 319,
         82, 286, 402, 174, 379,  95, 482, 216,
         86, 422, 367,  80,  29, 365, 335, 234,
        105, 466, 393, 331, 229, 388, 326, 230,
        282, 412,  28, 419,  65, 164, 172, 392
    }, 158};
}

LweCiphertext ZeroPrimeCiphertextFromOpenFHE() {
    return {{
        275,  25, 102, 390,   0, 198, 319, 255,
        414, 134, 452, 167, 262, 415, 449, 380,
        161, 425,   7, 469, 229,  19,  45,  19,
        357, 323, 453, 359, 191, 201,  22, 482,
        470, 400, 433, 363,  70, 132, 479, 250,
        257, 187, 269, 509, 205, 218, 358, 296,
        203, 283, 178, 296, 191,  97,  96, 119,
        338, 376,  38,  84, 265, 217,  79, 131
    }, 28};
}

} // namespace

int main() {
    if (controlled_reveal_reference::GateConstant(
            controlled_reveal_reference::GateAnd) != 448U) {
        return EXIT_FAILURE;
    }
    if (controlled_reveal_reference::GateConstant(
            controlled_reveal_reference::GateXor) != 384U) {
        return EXIT_FAILURE;
    }

    const auto and_range =
        controlled_reveal_reference::GateAccumulatorRange(
            controlled_reveal_reference::GateAnd);
    if (!and_range.swap || and_range.lb != 192U || and_range.ub != 448U) {
        return EXIT_FAILURE;
    }

    LweCiphertext prebootstrap = {};
    prebootstrap.b = 448U;
    const RingAccumulator and_accumulator =
        controlled_reveal_reference::InitGateAccumulator(
            controlled_reveal_reference::GateAnd, prebootstrap);
    if (and_accumulator.b.coeffs[0] !=
        controlled_reveal_reference::kAccumulatorNegativeMessage) {
        return EXIT_FAILURE;
    }
    if (and_accumulator.b.coeffs[2] !=
        controlled_reveal_reference::kAccumulatorPositiveMessage) {
        return EXIT_FAILURE;
    }
    if (and_accumulator.b.coeffs[1] != 0U ||
        and_accumulator.b.coeffs[3] != 0U) {
        return EXIT_FAILURE;
    }

    const RingPolynomial decomposed_513 =
        controlled_reveal_reference::SignedDigitDecomposePolynomial(513U);
    if (decomposed_513.coeffs[0] != 1U ||
        decomposed_513.coeffs[2] != 0U) {
        return EXIT_FAILURE;
    }

    RingPolynomial monomial_input = {};
    monomial_input.coeffs[0] = 7U;
    const RingPolynomial shifted =
        controlled_reveal_reference::MultiplyByNegacyclicMonomial(
            monomial_input, 513U);
    if (shifted.coeffs[1] !=
        controlled_reveal_reference::kRingModulus - 7U) {
        return EXIT_FAILURE;
    }

    controlled_reveal_reference::DecomposedAccumulator decomposed = {};
    decomposed.digits[0].coeffs[0] = 2U;
    controlled_reveal_reference::CggiBootstrapKeyRow key_row = {};
    key_row.digits[0].a.coeffs[0] = 3U;
    key_row.digits[0].b.coeffs[0] = 5U;
    const RingAccumulator product =
        controlled_reveal_reference::ExternalProductCGGI(
            decomposed, key_row);
    if (product.a.coeffs[0] != 6U || product.b.coeffs[0] != 10U) {
        return EXIT_FAILURE;
    }

    if (!controlled_reveal_reference::DecFixedHsk(
            OnePrimeCiphertextFromOpenFHE())) {
        return EXIT_FAILURE;
    }
    if (controlled_reveal_reference::DecFixedHsk(
            ZeroPrimeCiphertextFromOpenFHE())) {
        return EXIT_FAILURE;
    }

    static EvaluationKeys eval_keys = {};
    const IntegerCiphertext x = {};
    const IntegerCiphertext bound = {};
    const LweCiphertext predicate =
        controlled_reveal_reference::EvalLessOrEqualPredicate(
            x, bound, eval_keys);
    (void)predicate;
    (void)controlled_reveal_reference::ControlledReveal(x, bound, eval_keys);

    return EXIT_SUCCESS;
}

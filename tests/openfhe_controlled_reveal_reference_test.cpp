#include "src/gc/openfhe_controlled_reveal_reference.cpp"

#include <cstdlib>

namespace {

using controlled_reveal_reference::EvaluationKeys;
using controlled_reveal_reference::IntegerCiphertext;
using controlled_reveal_reference::LweCiphertext;
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
    if (and_accumulator.b[0] !=
        controlled_reveal_reference::kAccumulatorNegativeMessage) {
        return EXIT_FAILURE;
    }
    if (and_accumulator.b[2] !=
        controlled_reveal_reference::kAccumulatorPositiveMessage) {
        return EXIT_FAILURE;
    }
    if (and_accumulator.b[1] != 0U || and_accumulator.b[3] != 0U) {
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

#include "src/gc/openfhe_controlled_reveal_reference.cpp"

#include <cstdlib>

namespace {

using controlled_reveal_reference::IntegerCiphertext;
using controlled_reveal_reference::kIntegerBits;

IntegerCiphertext EncryptIntegerForReference(unsigned value) {
    IntegerCiphertext out = {};
    for (unsigned i = 0; i < kIntegerBits; ++i) {
        out.bits[i] =
            controlled_reveal_reference::EncryptBitWithFixedMask(
                ((value >> i) & 1U) != 0, i + 1U);
    }
    return out;
}

} // namespace

int main() {
    const IntegerCiphertext one = EncryptIntegerForReference(1);
    const IntegerCiphertext two = EncryptIntegerForReference(2);
    const IntegerCiphertext three = EncryptIntegerForReference(3);

    if (!controlled_reveal_reference::ControlledReveal(one, two)) {
        return EXIT_FAILURE;
    }
    if (controlled_reveal_reference::ControlledReveal(three, two)) {
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}

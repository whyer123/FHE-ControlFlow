#include "src/gc/openfhe_controlled_reveal_reference.cpp"

#include "binfhecontext.h"

#include <cstdlib>
#include <stdexcept>
#include <vector>

namespace {

void Require(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

std::vector<lbcrypto::LWECiphertext> EncryptIntegerBits(
    lbcrypto::BinFHEContext& cc,
    const lbcrypto::LWEPublicKey& hpk,
    uint64_t value,
    size_t bit_length) {
    std::vector<lbcrypto::LWECiphertext> result;
    result.reserve(bit_length);
    for (size_t i = 0; i < bit_length; ++i) {
        result.push_back(cc.Encrypt(hpk, static_cast<int>((value >> i) & 1U)));
    }
    return result;
}

} // namespace

int main() {
    try {
        lbcrypto::BinFHEContext cc;
        cc.GenerateBinFHEContext(lbcrypto::STD128);

        const auto hsk = cc.KeyGen();
        cc.BTKeyGen(hsk, lbcrypto::PUB_ENCRYPT);
        const auto hpk = cc.GetPublicKey();

        constexpr size_t kBitLength = 2;
        const auto one = EncryptIntegerBits(cc, hpk, 1, kBitLength);
        const auto two = EncryptIntegerBits(cc, hpk, 2, kBitLength);
        const auto three = EncryptIntegerBits(cc, hpk, 3, kBitLength);

        Require(controlled_reveal_reference::DecOfEvalLessOrEqual(
                    cc, hsk, one, two),
                "expected Enc(1) <= Enc(2)");
        Require(!controlled_reveal_reference::DecOfEvalLessOrEqual(
                    cc, hsk, three, two),
                "expected Enc(3) > Enc(2)");
    } catch (const std::exception&) {
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}

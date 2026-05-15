#include "binfhecontext-ser.h"
#include "binfhecontext.h"
#include "utils/serial.h"

#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

using namespace lbcrypto;

namespace {

constexpr size_t kDemoBitLength = 4;

void Require(bool ok, const std::string& message) {
    if (!ok) {
        throw std::runtime_error(message);
    }
}

std::vector<LWECiphertext> LoadCiphertextBits(
    const std::filesystem::path& key_dir,
    const std::string& prefix,
    size_t bit_length) {
    std::vector<LWECiphertext> bits;
    bits.reserve(bit_length);
    for (size_t i = 0; i < bit_length; ++i) {
        LWECiphertext ciphertext;
        const auto path = key_dir / (prefix + "_bit_" + std::to_string(i) +
                                     "_lwe_ciphertext.bin");
        Require(Serial::DeserializeFromFile(path.string(), ciphertext,
                                            SerType::BINARY),
                "failed to load ciphertext bit: " + path.string());
        bits.push_back(ciphertext);
    }
    return bits;
}

std::vector<LWECiphertext> AddEncryptedIntegerBits(
    BinFHEContext& cc,
    const std::vector<LWECiphertext>& lhs,
    const std::vector<LWECiphertext>& rhs) {
    Require(lhs.size() == rhs.size(), "encrypted add expects equal bit widths.");
    Require(!lhs.empty(), "encrypted add expects non-empty inputs.");

    std::vector<LWECiphertext> result(lhs.size());
    auto xor_bits = cc.EvalBinGate(XOR, lhs[0], rhs[0]);
    result[0] = xor_bits;
    auto carry = cc.EvalBinGate(AND, lhs[0], rhs[0]);

    for (size_t i = 1; i < lhs.size(); ++i) {
        xor_bits = cc.EvalBinGate(XOR, lhs[i], rhs[i]);
        result[i] = cc.EvalBinGate(XOR, xor_bits, carry);
        const auto generate = cc.EvalBinGate(AND, lhs[i], rhs[i]);
        const auto propagate = cc.EvalBinGate(AND, xor_bits, carry);
        carry = cc.EvalBinGate(XOR, generate, propagate);
    }

    return result;
}

} // namespace

int main(int argc, char** argv) {
    const std::filesystem::path key_dir =
        argc > 1 ? std::filesystem::path(argv[1])
                 : std::filesystem::path("demo_keys/openfhe_binfhe_demo_keypair");

    BinFHEContext cc;
    RingGSWACCKey refresh_key;
    LWESwitchingKey switch_key;

    Require(Serial::DeserializeFromFile(
                (key_dir / "binfhe_context_params.bin").string(), cc,
                SerType::BINARY),
            "failed to deserialize BinFHE context params.");
    Require(Serial::DeserializeFromFile(
                (key_dir / "eval_refresh_key.bin").string(), refresh_key,
                SerType::BINARY),
            "failed to deserialize refresh evaluation key.");
    Require(Serial::DeserializeFromFile(
                (key_dir / "eval_switch_key.bin").string(), switch_key,
                SerType::BINARY),
            "failed to deserialize switching evaluation key.");

    RingGSWBTKey eval_keys;
    eval_keys.BSkey = refresh_key;
    eval_keys.KSkey = switch_key;
    cc.BTKeyLoad(eval_keys);

    const auto a_prime =
        LoadCiphertextBits(key_dir, "a_prime", kDemoBitLength);
    const auto one_prime =
        LoadCiphertextBits(key_dir, "one_prime_integer", kDemoBitLength);

    const auto next_state =
        AddEncryptedIntegerBits(cc, a_prime, one_prime);
    Require(next_state.size() == kDemoBitLength,
            "encrypted add produced wrong bit length.");

    std::cout << "Evaluator material check loaded context, evaluation keys, "
                 "a', and integer one'.\n";
    std::cout << "Evaluator material check did not load hpk or hsk.\n";
    std::cout << "Computed one encrypted state update with OpenFHE EvalBinGate.\n";
    return 0;
}

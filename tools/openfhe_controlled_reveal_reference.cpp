#include "binfhecontext-ser.h"
#include "binfhecontext.h"
#include "utils/serial.h"

#include <filesystem>
#include <iostream>
#include <sstream>
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

void LoadEvaluationMaterialOnly(BinFHEContext& cc,
                                const std::filesystem::path& key_dir) {
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
}

LWEPrivateKey LoadSecretKeyForControlledReveal(
    const std::filesystem::path& key_dir) {
    LWEPrivateKey hsk;
    Require(Serial::DeserializeFromFile(
                (key_dir / "hsk_lwe_secret_key.bin").string(), hsk,
                SerType::BINARY),
            "failed to deserialize hsk.");
    return hsk;
}

LWECiphertext EvalEncryptedLessOrEqual(BinFHEContext& cc,
                                       const std::vector<LWECiphertext>& x,
                                       const std::vector<LWECiphertext>& bound) {
    Require(!x.empty(), "predicate inputs must be non-empty.");
    Require(x.size() == bound.size(),
            "predicate inputs must have equal bit widths.");

    auto not_bound0 = cc.EvalNOT(bound[0]);
    LWECiphertext greater = cc.EvalBinGate(AND, not_bound0, x[0]);

    for (size_t i = 1; i < x.size(); ++i) {
        auto not_bound_i = cc.EvalNOT(bound[i]);
        auto generate = cc.EvalBinGate(AND, not_bound_i, x[i]);
        auto xor_bits = cc.EvalBinGate(XOR, bound[i], x[i]);
        auto equal_bits = cc.EvalNOT(xor_bits);
        auto propagate = cc.EvalBinGate(AND, equal_bits, greater);

        // generate and propagate are mutually exclusive for this comparator.
        greater = cc.EvalBinGate(XOR, generate, propagate);
    }

    return cc.EvalNOT(greater);
}

bool ControlledRevealPredicateOnly(BinFHEContext& cc,
                                   const LWEPrivateKey& hsk,
                                   const std::vector<LWECiphertext>& x,
                                   const std::vector<LWECiphertext>& bound) {
    // This is the exact reference expression we later need to garble:
    //
    //     Dec_hsk(OpenFHE.Eval([x <= b], x', b'))
    //
    // The evaluator must not receive a reusable Dec_hsk(c') oracle. The
    // decryption is tied to the predicate ciphertext produced here.
    auto predicate_ciphertext = EvalEncryptedLessOrEqual(cc, x, bound);

    LWEPlaintext predicate_bit = 0;
    cc.Decrypt(hsk, predicate_ciphertext, &predicate_bit);
    return predicate_bit == 1;
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

        // generate and propagate are mutually exclusive in a full adder.
        carry = cc.EvalBinGate(XOR, generate, propagate);
    }

    return result;
}

std::string JoinPredicateBits(const std::vector<bool>& bits) {
    std::ostringstream out;
    for (size_t i = 0; i < bits.size(); ++i) {
        if (i > 0) {
            out << ",";
        }
        out << (bits[i] ? 1 : 0);
    }
    return out.str();
}

} // namespace

int main(int argc, char** argv) {
    try {
        const std::filesystem::path key_dir =
            argc > 1
                ? std::filesystem::path(argv[1])
                : std::filesystem::path("demo_keys/openfhe_binfhe_demo_keypair");

        BinFHEContext cc;
        LoadEvaluationMaterialOnly(cc, key_dir);
        const auto hsk = LoadSecretKeyForControlledReveal(key_dir);

        auto state = LoadCiphertextBits(key_dir, "a_prime", kDemoBitLength);
        const auto bound = LoadCiphertextBits(key_dir, "b_prime", kDemoBitLength);
        const auto one =
            LoadCiphertextBits(key_dir, "one_prime_integer", kDemoBitLength);

        std::vector<bool> predicate_sequence;
        size_t iterations = 0;

        while (true) {
            const bool predicate =
                ControlledRevealPredicateOnly(cc, hsk, state, bound);
            predicate_sequence.push_back(predicate);

            if (!predicate) {
                break;
            }

            ++iterations;
            state = AddEncryptedIntegerBits(cc, state, one);
        }

        std::cout << "--- OpenFHE Controlled Reveal Reference ---\n";
        std::cout << "Reference expression: "
                  << "Dec_hsk(OpenFHE.Eval([x <= b], x', b'))\n";
        std::cout << "OpenFHE EvalBinGate is inside the predicate computation.\n";
        std::cout << "hpk was not loaded by this reference program.\n";
        std::cout << "hsk was loaded only to perform the final controlled reveal.\n";
        std::cout << "Predicate sequence: "
                  << JoinPredicateBits(predicate_sequence) << "\n";
        std::cout << "Encrypted loop iterations executed: "
                  << iterations << "\n";
    } catch (const std::exception& ex) {
        std::cerr << "openfhe_controlled_reveal_reference failed: "
                  << ex.what() << "\n";
        return 1;
    }

    return 0;
}

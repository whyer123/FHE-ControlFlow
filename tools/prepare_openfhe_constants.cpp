#include "binfhecontext-ser.h"
#include "binfhecontext.h"
#include "utils/serial.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

using namespace lbcrypto;

namespace {

void Require(bool ok, const std::string& message) {
    if (!ok) {
        throw std::runtime_error(message);
    }
}

void WriteTextFile(const std::filesystem::path& path, const std::string& body) {
    std::ofstream file(path);
    Require(file.good(), "failed to open " + path.string());
    file << body;
}

LWECiphertext EncryptAndVerifyBit(BinFHEContext& cc,
                                  const LWEPublicKey& hpk,
                                  const LWEPrivateKey& hsk,
                                  int bit,
                                  const std::string& label) {
    auto ciphertext = cc.Encrypt(hpk, bit);
    LWEPlaintext decrypted = 0;
    cc.Decrypt(hsk, ciphertext, &decrypted);
    Require(decrypted == bit, label + " verification failed.");
    return ciphertext;
}

void WriteCiphertext(const std::filesystem::path& json_path,
                     const std::filesystem::path& bin_path,
                     const LWECiphertext& ciphertext,
                     const std::string& label) {
    Require(Serial::SerializeToFile(json_path.string(), ciphertext,
                                    SerType::JSON),
            "failed to serialize " + label + " JSON.");
    Require(Serial::SerializeToFile(bin_path.string(), ciphertext,
                                    SerType::BINARY),
            "failed to serialize " + label + " binary.");
}

void WriteIntegerBits(BinFHEContext& cc,
                      const LWEPublicKey& hpk,
                      const LWEPrivateKey& hsk,
                      const std::filesystem::path& key_dir,
                      const std::string& prefix,
                      uint64_t value,
                      size_t bit_length) {
    for (size_t i = 0; i < bit_length; ++i) {
        const int bit = ((value >> i) & 1U) != 0 ? 1 : 0;
        auto ciphertext = EncryptAndVerifyBit(
            cc, hpk, hsk, bit, prefix + "_bit_" + std::to_string(i));
        WriteCiphertext(
            key_dir / (prefix + "_bit_" + std::to_string(i) + "_lwe_ciphertext.json"),
            key_dir / (prefix + "_bit_" + std::to_string(i) + "_lwe_ciphertext.bin"),
            ciphertext,
            prefix + "_bit_" + std::to_string(i));
    }
}

} // namespace

int main(int argc, char** argv) {
    const std::filesystem::path key_dir =
        argc > 1 ? std::filesystem::path(argv[1])
                 : std::filesystem::path("demo_keys/openfhe_binfhe_demo_keypair");

    BinFHEContext cc;
    LWEPublicKey hpk;
    LWEPrivateKey hsk;
    RingGSWACCKey refresh_key;
    LWESwitchingKey switch_key;

    Require(Serial::DeserializeFromFile(
                (key_dir / "binfhe_context_params.bin").string(), cc,
                SerType::BINARY),
            "failed to deserialize BinFHE context params.");
    Require(Serial::DeserializeFromFile(
                (key_dir / "hpk_lwe_public_key.bin").string(), hpk,
                SerType::BINARY),
            "failed to deserialize hpk.");
    Require(Serial::DeserializeFromFile(
                (key_dir / "hsk_lwe_secret_key.bin").string(), hsk,
                SerType::BINARY),
            "failed to deserialize hsk for verification.");
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
    eval_keys.Pkey = hpk;
    cc.BTKeyLoad(eval_keys);

    constexpr size_t kDemoBitLength = 4;
    constexpr uint64_t kDemoA = 3;
    constexpr uint64_t kDemoB = 7;

    auto one_prime = EncryptAndVerifyBit(cc, hpk, hsk, 1, "one_prime");

    const auto one_json = key_dir / "one_prime_lwe_ciphertext.json";
    const auto one_bin = key_dir / "one_prime_lwe_ciphertext.bin";
    const auto constants_manifest = key_dir / "encrypted_constants_manifest.md";

    WriteCiphertext(one_json, one_bin, one_prime, "one_prime");
    WriteIntegerBits(cc, hpk, hsk, key_dir, "a_prime", kDemoA, kDemoBitLength);
    WriteIntegerBits(cc, hpk, hsk, key_dir, "b_prime", kDemoB, kDemoBitLength);
    WriteIntegerBits(cc, hpk, hsk, key_dir, "one_prime_integer",
                     1, kDemoBitLength);

    WriteTextFile(
        constants_manifest,
        "# OpenFHE Demo Encrypted Constants\n\n"
        "This directory includes fixed encrypted endpoints and constants for "
        "the offline loop demo.\n\n"
        "## Fixed integer material\n\n"
        "- Demo bit length: `4`\n"
        "- `a = 3`: `a_prime_bit_0..3_lwe_ciphertext.*`\n"
        "- `b = 7`: `b_prime_bit_0..3_lwe_ciphertext.*`\n"
        "- Integer `one' = Enc(1)`: "
        "`one_prime_integer_bit_0..3_lwe_ciphertext.*`\n"
        "- Verification: every bit was checked with `hsk` during setup.\n\n"
        "## one_prime\n\n"
        "- Plaintext: `1`\n"
        "- Encryption key: `hpk_lwe_public_key.*`\n"
        "- Ciphertext JSON: `one_prime_lwe_ciphertext.json`\n"
        "- Ciphertext binary: `one_prime_lwe_ciphertext.bin`\n"
        "- Verification: `Dec_hsk(one_prime) = 1`\n\n"
        "Evaluator loop usage:\n\n"
        "```text\n"
        "x' <- FHE.Add(x', one')\n"
        "```\n\n"
        "The evaluator should receive integer `one'` as prepared ciphertext "
        "bits and does not need `hpk` to encrypt the constant itself. In the "
        "fixed-bound GC runtime, `b'` is setup material for `GC_f`; it is not "
        "a free runtime input.\n");

    std::cout << "Prepared one_prime in " << key_dir.string() << "\n";
    std::cout << "  one_prime json: " << one_json.string() << "\n";
    std::cout << "  one_prime bin: " << one_bin.string() << "\n";
    std::cout << "  a_prime/b_prime/one_prime_integer bits: "
              << kDemoBitLength << " each\n";
    return 0;
}

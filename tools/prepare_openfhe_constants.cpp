#include "binfhecontext-ser.h"
#include "binfhecontext.h"
#include "utils/serial.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>

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

    auto one_prime = cc.Encrypt(hpk, 1);

    LWEPlaintext decrypted = 0;
    cc.Decrypt(hsk, one_prime, &decrypted);
    Require(decrypted == 1, "one_prime verification failed.");

    const auto one_json = key_dir / "one_prime_lwe_ciphertext.json";
    const auto one_bin = key_dir / "one_prime_lwe_ciphertext.bin";
    const auto constants_manifest = key_dir / "encrypted_constants_manifest.md";

    Require(Serial::SerializeToFile(one_json.string(), one_prime, SerType::JSON),
            "failed to serialize one_prime JSON.");
    Require(Serial::SerializeToFile(one_bin.string(), one_prime, SerType::BINARY),
            "failed to serialize one_prime binary.");

    WriteTextFile(
        constants_manifest,
        "# OpenFHE Demo Encrypted Constants\n\n"
        "This directory includes fixed encrypted constants for the offline loop "
        "demo.\n\n"
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
        "The evaluator should receive `one'` as a prepared ciphertext and does "
        "not need `hpk` to encrypt the constant itself.\n");

    std::cout << "Prepared one_prime in " << key_dir.string() << "\n";
    std::cout << "  one_prime json: " << one_json.string() << "\n";
    std::cout << "  one_prime bin: " << one_bin.string() << "\n";
    return 0;
}

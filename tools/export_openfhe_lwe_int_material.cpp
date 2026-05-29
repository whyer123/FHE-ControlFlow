#include "binfhecontext-ser.h"
#include "binfhecontext.h"
#include "utils/serial.h"

#include <cstdint>
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

uint64_t ToU64(const NativeInteger& value) {
    return value.ConvertToInt();
}

uint64_t AddMod(uint64_t lhs, uint64_t rhs, uint64_t modulus) {
    return (lhs + rhs) % modulus;
}

uint64_t MulMod(uint64_t lhs, uint64_t rhs, uint64_t modulus) {
    return static_cast<uint64_t>(
        (static_cast<unsigned __int128>(lhs) * rhs) % modulus);
}

uint64_t ParsePlaintext(const char* value,
                        uint64_t plaintext_modulus,
                        const std::string& label) {
    const auto plaintext = std::stoull(value);
    Require(plaintext < plaintext_modulus,
            label + " must be smaller than plaintext modulus.");
    return plaintext;
}

uint64_t DecodeWithExportedSecret(const LWEPrivateKey& hsk,
                                  const LWECiphertext& ciphertext,
                                  uint64_t plaintext_modulus) {
    Require(hsk != nullptr, "hsk is null.");
    Require(ciphertext != nullptr, "ciphertext is null.");

    auto secret = hsk->GetElement();
    const auto& mask = ciphertext->GetA();
    const auto q_native = ciphertext->GetModulus();
    const uint64_t q = ToU64(q_native);
    secret.SwitchModulus(q_native);
    Require(secret.GetLength() == mask.GetLength(),
            "hsk/ciphertext dimension mismatch.");
    Require(q > 0, "ciphertext modulus is zero.");

    uint64_t pad = 0;
    for (uint32_t i = 0; i < mask.GetLength(); ++i) {
        pad = AddMod(pad, MulMod(ToU64(mask[i]), ToU64(secret[i]), q), q);
    }

    const uint64_t body = ToU64(ciphertext->GetB());
    const uint64_t phase = (body + q - pad) % q;
    const uint64_t rounded = (phase + q / (2 * plaintext_modulus)) % q;
    return (plaintext_modulus * rounded) / q;
}

LWECiphertext EncryptVerifyAndExport(BinFHEContext& cc,
                                     const LWEPublicKey& hpk,
                                     const LWEPrivateKey& hsk,
                                     uint64_t plaintext,
                                     uint64_t plaintext_modulus,
                                     const std::string& label) {
    auto ciphertext = cc.Encrypt(hpk, plaintext, SMALL_DIM,
                                 plaintext_modulus);
    Require(ciphertext != nullptr, label + " encryption failed.");
    Require(ToU64(ciphertext->GetptModulus()) == plaintext_modulus,
            label + " ciphertext plaintext modulus mismatch.");

    LWEPlaintext openfhe_dec = 0;
    cc.Decrypt(hsk, ciphertext, &openfhe_dec, plaintext_modulus);
    Require(openfhe_dec == plaintext,
            label + " OpenFHE decrypt verification failed.");

    const auto manual_dec =
        DecodeWithExportedSecret(hsk, ciphertext, plaintext_modulus);
    Require(manual_dec == plaintext,
            label + " exported-field decrypt verification failed.");
    return ciphertext;
}

void WriteVector(std::ostream& out,
                 const std::string& name,
                 const NativeVector& vector) {
    out << name << " = [";
    for (uint32_t i = 0; i < vector.GetLength(); ++i) {
        if (i != 0) {
            out << ", ";
        }
        out << ToU64(vector[i]);
    }
    out << "]\n";
}

void WriteCiphertextWithDec(std::ostream& out,
                            const std::string& prefix,
                            const LWEPrivateKey& hsk,
                            const LWECiphertext& ciphertext,
                            uint64_t plaintext,
                            uint64_t plaintext_modulus) {
    out << prefix << ".plaintext = " << plaintext << "\n";
    out << prefix << ".dimension = " << ciphertext->GetLength() << "\n";
    out << prefix << ".ciphertext_modulus = "
        << ToU64(ciphertext->GetModulus()) << "\n";
    out << prefix << ".plaintext_modulus = "
        << ToU64(ciphertext->GetptModulus()) << "\n";
    out << prefix << ".body = " << ToU64(ciphertext->GetB()) << "\n";
    WriteVector(out, prefix + ".a", ciphertext->GetA());
    out << prefix << ".manual_dec = "
        << DecodeWithExportedSecret(hsk, ciphertext, plaintext_modulus)
        << "\n\n";
}

void WriteManifest(const std::filesystem::path& path,
                   uint64_t plaintext_modulus,
                   uint64_t a_plaintext,
                   uint64_t b_plaintext,
                   uint64_t one_plaintext,
                   const LWEPrivateKey& hsk,
                   const LWECiphertext& a_prime,
                   const LWECiphertext& b_prime,
                   const LWECiphertext& one_prime) {
    std::ofstream out(path);
    Require(out.good(), "failed to open output manifest.");

    const auto& secret = hsk->GetElement();
    auto switched_secret = secret;
    switched_secret.SwitchModulus(a_prime->GetModulus());
    out << "# OpenFHE LWE Integer Material Export\n\n";
    out << "This file is setup-side material for the v2 decrypt-compare path.\n";
    out << "It exports real OpenFHE-generated LWE integer ciphertext fields and "
           "the matching fixed hsk coefficients for GC setup.\n\n";
    out << "logical_plaintext_bits = 4\n";
    out << "plaintext_modulus = " << plaintext_modulus << "\n";
    out << "hsk.dimension = " << secret.GetLength() << "\n";
    out << "hsk.modulus = " << ToU64(secret.GetModulus()) << "\n";
    WriteVector(out, "hsk.s_raw", secret);
    out << "hsk.switched_modulus = "
        << ToU64(switched_secret.GetModulus()) << "\n";
    WriteVector(out, "hsk.s_mod_q", switched_secret);
    out << "\n";

    WriteCiphertextWithDec(out, "a_prime", hsk, a_prime, a_plaintext,
                           plaintext_modulus);
    WriteCiphertextWithDec(out, "b_prime", hsk, b_prime, b_plaintext,
                           plaintext_modulus);
    WriteCiphertextWithDec(out, "one_prime", hsk, one_prime, one_plaintext,
                           plaintext_modulus);
}

} // namespace

int main(int argc, char** argv) {
    const std::filesystem::path key_dir =
        argc > 1 ? std::filesystem::path(argv[1])
                 : std::filesystem::path("demo_keys/openfhe_binfhe_demo_keypair");
    const std::filesystem::path output_path =
        argc > 2 ? std::filesystem::path(argv[2])
                 : key_dir / "v2_lwe_integer_material.txt";

    constexpr uint64_t kPlaintextModulus = 16;
    Require(argc == 1 || argc == 2 || argc == 3 || argc == 5 || argc == 6,
            "usage: export_openfhe_lwe_int_material [key-dir] [output-path] "
            "[a_plaintext b_plaintext [one_plaintext]]");
    const uint64_t a_plaintext =
        argc > 3 ? ParsePlaintext(argv[3], kPlaintextModulus, "a_plaintext")
                 : 3;
    const uint64_t b_plaintext =
        argc > 4 ? ParsePlaintext(argv[4], kPlaintextModulus, "b_plaintext")
                 : 7;
    const uint64_t one_plaintext =
        argc > 5 ? ParsePlaintext(argv[5], kPlaintextModulus, "one_plaintext")
                 : 1;

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
            "failed to deserialize hsk.");
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

    auto a_prime = EncryptVerifyAndExport(cc, hpk, hsk, a_plaintext,
                                          kPlaintextModulus, "a_prime");
    auto b_prime = EncryptVerifyAndExport(cc, hpk, hsk, b_plaintext,
                                          kPlaintextModulus, "b_prime");
    auto one_prime = EncryptVerifyAndExport(cc, hpk, hsk, one_plaintext,
                                            kPlaintextModulus, "one_prime");

    WriteManifest(output_path, kPlaintextModulus,
                  a_plaintext, b_plaintext, one_plaintext, hsk,
                  a_prime, b_prime, one_prime);

    std::cout << "Exported OpenFHE LWE integer material to "
              << output_path.string() << "\n";
    std::cout << "  plaintext modulus: " << kPlaintextModulus << "\n";
    std::cout << "  ciphertext dimension: " << a_prime->GetLength() << "\n";
    std::cout << "  ciphertext modulus: "
              << ToU64(a_prime->GetModulus()) << "\n";
    return 0;
}

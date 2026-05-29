#include "binfhecontext-ser.h"
#include "binfhecontext.h"
#include "utils/serial.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
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

BINFHE_PARAMSET ParseParamSet(const std::string& value) {
    if (value == "TOY") {
        return TOY;
    }
    if (value == "MEDIUM") {
        return MEDIUM;
    }
    if (value == "STD128") {
        return STD128;
    }
    if (value == "STD192") {
        return STD192;
    }
    if (value == "STD256") {
        return STD256;
    }
    throw std::runtime_error(
        "unsupported BinFHE paramset '" + value +
        "'. Supported values: TOY, MEDIUM, STD128, STD192, STD256.");
}

std::string ParamSetName(BINFHE_PARAMSET param_set) {
    switch (param_set) {
    case TOY:
        return "TOY";
    case MEDIUM:
        return "MEDIUM";
    case STD128:
        return "STD128";
    case STD192:
        return "STD192";
    case STD256:
        return "STD256";
    default:
        return "UNKNOWN";
    }
}

} // namespace

int Run(int argc, char** argv) {
    Require(argc >= 1 && argc <= 3,
            "usage: generate_openfhe_keypair [output-dir] [paramset]");

    const std::filesystem::path output_dir =
        argc > 1 ? std::filesystem::path(argv[1])
                 : std::filesystem::path("demo_keys/openfhe_binfhe_demo_keypair");
    const auto param_set = argc > 2 ? ParseParamSet(argv[2]) : TOY;
    const auto param_set_name = ParamSetName(param_set);

    std::filesystem::create_directories(output_dir);

    BinFHEContext cc;
    cc.GenerateBinFHEContext(param_set);

    auto hsk = cc.KeyGen();
    Require(hsk != nullptr, "OpenFHE BinFHE KeyGen failed.");

    // Public-key mode creates the public encryption key plus gate evaluation
    // material needed for public-key encrypted BinFHE ciphertexts.
    cc.BTKeyGen(hsk, PUB_ENCRYPT);
    auto hpk = cc.GetPublicKey();
    Require(hpk != nullptr, "OpenFHE BinFHE public key generation failed.");

    auto ct0 = cc.Encrypt(hpk, 0);
    auto ct1 = cc.Encrypt(hpk, 1);
    auto ct1_independent = cc.Encrypt(hpk, 1);

    LWEPlaintext dec0 = 0;
    LWEPlaintext dec1 = 0;
    cc.Decrypt(hsk, ct0, &dec0);
    cc.Decrypt(hsk, ct1, &dec1);
    std::cout << "Public-key self-test decryptions: Dec(Enc(0))="
              << dec0 << ", Dec(Enc(1))=" << dec1 << "\n";
    Require(dec0 == 0, "public-key encryption self-test failed for bit 0.");
    Require(dec1 == 1, "public-key encryption self-test failed for bit 1.");

    auto ct_and = cc.EvalBinGate(AND, ct1, ct1_independent);
    LWEPlaintext dec_and = 0;
    cc.Decrypt(hsk, ct_and, &dec_and);
    Require(dec_and == 1, "bootstrapped AND self-test failed.");

    const auto hpk_json = output_dir / "hpk_lwe_public_key.json";
    const auto hpk_bin = output_dir / "hpk_lwe_public_key.bin";
    const auto hsk_json = output_dir / "hsk_lwe_secret_key.json";
    const auto hsk_bin = output_dir / "hsk_lwe_secret_key.bin";
    const auto context_bin = output_dir / "binfhe_context_params.bin";
    const auto refresh_key_bin = output_dir / "eval_refresh_key.bin";
    const auto switch_key_bin = output_dir / "eval_switch_key.bin";
    const auto manifest = output_dir / "manifest.md";

    Require(Serial::SerializeToFile(hpk_json.string(), hpk,
                                    SerType::JSON),
            "failed to serialize public key JSON.");
    Require(Serial::SerializeToFile(hpk_bin.string(), hpk,
                                    SerType::BINARY),
            "failed to serialize public key binary.");
    Require(Serial::SerializeToFile(hsk_json.string(), hsk,
                                    SerType::JSON),
            "failed to serialize secret key JSON.");
    Require(Serial::SerializeToFile(hsk_bin.string(), hsk,
                                    SerType::BINARY),
            "failed to serialize secret key binary.");
    Require(Serial::SerializeToFile(context_bin.string(), cc, SerType::BINARY),
            "failed to serialize BinFHE context params binary.");
    Require(Serial::SerializeToFile(refresh_key_bin.string(), cc.GetRefreshKey(),
                                    SerType::BINARY),
            "failed to serialize refresh evaluation key binary.");
    Require(Serial::SerializeToFile(switch_key_bin.string(), cc.GetSwitchKey(),
                                    SerType::BINARY),
            "failed to serialize switching evaluation key binary.");

    std::ostringstream manifest_body;
    manifest_body
        << "# OpenFHE BinFHE Demo Key Pair\n\n"
        << "This directory contains one generated OpenFHE BinFHE/LWE key pair for "
        << "the fixed demo direction.\n\n"
        << "## Files\n\n"
        << "- `hpk_lwe_public_key.json`: JSON-serialized LWE public key.\n"
        << "- `hpk_lwe_public_key.bin`: binary-serialized LWE public key.\n"
        << "- `hsk_lwe_secret_key.json`: JSON-serialized LWE secret key.\n"
        << "- `hsk_lwe_secret_key.bin`: binary-serialized LWE secret key.\n"
        << "- `binfhe_context_params.bin`: BinFHE context/parameter material.\n"
        << "- `eval_refresh_key.bin`: refresh/bootstrapping evaluation key.\n"
        << "- `eval_switch_key.bin`: switching evaluation key.\n\n"
        << "## Parameters\n\n"
        << "- OpenFHE context: `BinFHEContext`\n"
        << "- Parameter set: `" << param_set_name << "`\n"
        << "- Public-key encryption self-test: `Enc_hpk(0/1)` then `Dec_hsk`\n"
        << "- Gate self-test: `EvalBinGate(AND, Enc_hpk(1), Enc_hpk(1)) = 1`\n\n"
        << "## Size note\n\n"
        << "The LWE public key is large because it contains many LWE public-key "
        << "samples. The JSON file expands vectors into decimal text and is much "
        << "larger than the binary form; runtime code should prefer `.bin` files. "
        << "The switching evaluation key is also large and is separate from `hpk`.\n\n"
        << "These files are demo material only. Use `STD128` or stronger when the "
        << "demo needs normal OpenFHE security parameters; `TOY` is only for fast "
        << "prototype runs.\n";

    WriteTextFile(manifest, manifest_body.str());

    std::cout << "Generated OpenFHE BinFHE key pair in "
              << output_dir.string() << "\n";
    std::cout << "  paramset: " << param_set_name << "\n";
    std::cout << "  hpk: " << hpk_json.string() << "\n";
    std::cout << "  hsk: " << hsk_json.string() << "\n";
    std::cout << "  context params: " << context_bin.string() << "\n";
    std::cout << "  refresh eval key: " << refresh_key_bin.string() << "\n";
    std::cout << "  switch eval key: " << switch_key_bin.string() << "\n";
    return 0;
}

int main(int argc, char** argv) {
    try {
        return Run(argc, argv);
    } catch (const std::exception& e) {
        std::cerr << e.what() << "\n";
        return 1;
    }
}

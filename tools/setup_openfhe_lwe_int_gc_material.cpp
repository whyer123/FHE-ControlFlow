#include "src/gc/active_garbled_circuit_io.h"
#include "src/gc/active_garbled_circuit.h"
#include "src/gc/boolean_circuit_export.h"
#include "src/gc/boolean_circuit_io.h"
#include "src/gc/openfhe_lwe_int_decrypt_compare_circuit.h"
#include "src/gc/openfhe_lwe_int_material.h"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

void Require(bool condition, const std::string& message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

uint64_t AddMod(uint64_t lhs, uint64_t rhs, uint64_t modulus) {
    return static_cast<uint64_t>(
        (static_cast<unsigned __int128>(lhs) + rhs) % modulus);
}

uint64_t MulMod(uint64_t lhs, uint64_t rhs, uint64_t modulus) {
    return static_cast<uint64_t>(
        (static_cast<unsigned __int128>(lhs) * rhs) % modulus);
}

uint64_t DecodeWithSetupSecret(
    const OpenFHELWEIntCiphertextMaterial& ciphertext,
    const std::vector<uint64_t>& hsk_mod_q) {
    Require(ciphertext.a.size() == hsk_mod_q.size(),
            "ciphertext/hsk dimension mismatch during setup decode.");
    const uint64_t q = ciphertext.ciphertext_modulus;
    const uint64_t p = ciphertext.plaintext_modulus;
    Require(q > 0, "ciphertext modulus is zero during setup decode.");
    Require(p > 0, "plaintext modulus is zero during setup decode.");

    uint64_t pad = 0;
    for (size_t i = 0; i < ciphertext.a.size(); ++i) {
        pad = AddMod(pad, MulMod(ciphertext.a[i], hsk_mod_q[i], q), q);
    }

    const uint64_t phase = (ciphertext.body + q - pad) % q;
    const uint64_t rounded = (phase + q / (2U * p)) % q;
    return static_cast<uint64_t>(
        (static_cast<unsigned __int128>(p) * rounded) / q);
}

OpenFHELWEIntCiphertextMaterial AddCiphertextsModQ(
    const OpenFHELWEIntCiphertextMaterial& lhs,
    const OpenFHELWEIntCiphertextMaterial& rhs,
    const std::vector<uint64_t>& hsk_mod_q) {
    auto out = lhs;
    Require(lhs.dimension == rhs.dimension,
            "setup ciphertext add dimension mismatch.");
    Require(lhs.ciphertext_modulus == rhs.ciphertext_modulus,
            "setup ciphertext add modulus mismatch.");
    Require(lhs.plaintext_modulus == rhs.plaintext_modulus,
            "setup ciphertext add plaintext modulus mismatch.");
    Require(lhs.a.size() == rhs.a.size(),
            "setup ciphertext add a-vector length mismatch.");

    for (size_t i = 0; i < out.a.size(); ++i) {
        out.a[i] = AddMod(lhs.a[i], rhs.a[i], lhs.ciphertext_modulus);
    }
    out.body = AddMod(lhs.body, rhs.body, lhs.ciphertext_modulus);
    out.plaintext = (lhs.plaintext + rhs.plaintext) % lhs.plaintext_modulus;
    out.manual_dec = DecodeWithSetupSecret(out, hsk_mod_q);
    return out;
}

std::string NoiseUnsafeMessage(uint64_t expected,
                               uint64_t decoded,
                               size_t step) {
    std::ostringstream out;
    out << "noise-unsafe OpenFHE integer loop material: "
        << "after " << step << " encrypted increments expected Dec(x')="
        << expected << " but decoded " << decoded << ".";
    return out.str();
}

void ValidateRuntimeLoopBounds(const OpenFHELWEIntMaterial& material) {
    Require(material.one_prime.plaintext == 1,
            "first v2 runtime demo requires one'=Enc(1).");
    if (material.a_prime.plaintext <= material.b_prime.plaintext) {
        Require(material.b_prime.plaintext + material.one_prime.plaintext <
                    material.plaintext_modulus,
                "wraparound-unsafe OpenFHE integer loop material: "
                "b + one must stay below plaintext modulus.");

        auto state = material.a_prime;
        const uint64_t total_steps =
            material.b_prime.plaintext - material.a_prime.plaintext + 1U;
        for (uint64_t step = 0; step <= total_steps; ++step) {
            const uint64_t expected = material.a_prime.plaintext + step;
            const uint64_t decoded =
                DecodeWithSetupSecret(state, material.hsk_mod_q);
            Require(decoded == expected,
                    NoiseUnsafeMessage(expected, decoded,
                                       static_cast<size_t>(step)));
            if (step != total_steps) {
                state = AddCiphertextsModQ(state, material.one_prime,
                                           material.hsk_mod_q);
            }
        }
    }
}

void WriteManifest(const std::filesystem::path& path,
                   const OpenFHELWEIntMaterial& material,
                   const BooleanCircuit& circuit) {
    std::ofstream out(path);
    Require(out.good(), "failed to open v2 runtime manifest: " + path.string());

    out << "# OpenFHE LWE Integer GC Runtime Material\n\n"
        << "- Functionality: `GC_f(x', b') = GC{ [Dec_hsk(x') <= Dec_hsk(b')] }`.\n"
        << "- Runtime material contains OpenFHE-exported LWE integer ciphertext fields only.\n"
        << "- Runtime material does not contain `hpk`, `hsk`, clear plaintext, or manual decryption values.\n"
        << "- Secret key coefficients are embedded only as selected labels in the GC artifact.\n"
        << "- Demo plaintext modulus: `" << material.plaintext_modulus << "`.\n"
        << "- LWE dimension: `" << material.hsk_dimension << "`.\n"
        << "- Circuit input wires: `" << circuit.input_wires.size() << "`.\n"
        << "- Circuit gates: `" << circuit.gates.size() << "`.\n"
        << "- GC backend: " << ActiveGarbledCircuitBackendName() << ".\n";
}

} // namespace

int main(int argc, char** argv) {
    try {
        Require(argc == 3,
                "usage: setup_openfhe_lwe_int_gc_material <full-material.txt> <output-dir>");

        const auto full_material_path = std::filesystem::path(argv[1]);
        const auto output_dir = std::filesystem::path(argv[2]);
        std::filesystem::create_directories(output_dir);

        const auto material = ReadOpenFHELWEIntMaterial(full_material_path.string());
        ValidateRuntimeLoopBounds(material);
        const auto params = material.CircuitParams();
        const auto circuit = OpenFHELWEIntDecryptCompareCircuit::Describe(params);

        const auto circuit_text =
            output_dir / "openfhe_lwe_int_circuit_g_demo.txt";
        const auto circuit_shape =
            output_dir / "openfhe_lwe_int_circuit_shape.bin";
        const auto artifact_path =
            output_dir / "openfhe_lwe_int_gc_artifact.bin";
        const auto runtime_material_path =
            output_dir / "openfhe_lwe_int_runtime_material.txt";

        WriteBooleanCircuitText(circuit, circuit_text.string(),
                                true /* redact_constant_values */);
        WriteBooleanCircuitShape(circuit, circuit_shape.string());

        const auto artifact = GarbleActiveCircuit(circuit);
        WriteActiveGarbledCircuitArtifact(artifact, artifact_path.string());
        WriteOpenFHELWEIntRuntimeMaterial(ToRuntimeMaterial(material),
                                          runtime_material_path.string());
        WriteManifest(output_dir / "openfhe_lwe_int_runtime_manifest.md",
                      material, circuit);

        std::cout << "Prepared OpenFHE LWE integer GC runtime material in "
                  << output_dir.string() << "\n";
        std::cout << "Circuit dump: " << circuit_text.string() << "\n";
        std::cout << "Circuit shape: " << circuit_shape.string() << "\n";
        std::cout << "GC artifact: " << artifact_path.string() << "\n";
        std::cout << "Runtime material: " << runtime_material_path.string() << "\n";
        std::cout << "Runtime public input wires: "
                  << circuit.input_wires.size() << "\n";
        return EXIT_SUCCESS;
    } catch (const std::exception& error) {
        std::cerr << error.what() << "\n";
        return EXIT_FAILURE;
    }
}

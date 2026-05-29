#include "src/gc/active_garbled_circuit_io.h"
#include "src/gc/boolean_circuit_export.h"
#include "src/gc/boolean_circuit_io.h"
#include "src/gc/openfhe_lwe_int_decrypt_compare_circuit.h"
#include "src/gc/openfhe_lwe_int_material.h"
#ifdef USE_EMP_GC
#include "src/gc/emp_garbled_circuit.h"
#else
#include "src/gc/minimal_garbled_circuit.h"
#endif

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {

void Require(bool condition, const std::string& message) {
    if (!condition) {
        throw std::runtime_error(message);
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
#ifdef USE_EMP_GC
        << "- GC backend: EMP half-gates.\n";
#else
        << "- GC backend: in-repo minimal fallback.\n";
#endif
}

} // namespace

int main(int argc, char** argv) {
    Require(argc == 3,
            "usage: setup_openfhe_lwe_int_gc_material <full-material.txt> <output-dir>");

    const auto full_material_path = std::filesystem::path(argv[1]);
    const auto output_dir = std::filesystem::path(argv[2]);
    std::filesystem::create_directories(output_dir);

    const auto material = ReadOpenFHELWEIntMaterial(full_material_path.string());
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

#ifdef USE_EMP_GC
    EmpGarbledCircuit garbler;
#else
    MinimalGarbledCircuit garbler;
#endif
    const auto artifact = garbler.Garble(circuit);
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
}

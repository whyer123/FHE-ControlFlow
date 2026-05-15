#include "src/fhe/fhe_context.h"
#include "src/gates/fhe_gates.h"
#include "src/gc/active_garbled_circuit_io.h"
#include "src/gc/boolean_circuit_export.h"
#include "src/gc/boolean_circuit_io.h"
#include "src/gc/controlled_reveal_circuit.h"
#ifdef USE_EMP_GC
#include "src/gc/emp_garbled_circuit.h"
#else
#include "src/gc/minimal_garbled_circuit.h"
#endif

#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

constexpr size_t kBitLength = 4;
constexpr uint64_t kFixedStart = 3;
constexpr uint64_t kFixedBound = 7;

void Require(bool condition, const std::string& message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

void WriteMockEncryptedBits(const std::filesystem::path& path,
                            uint64_t value,
                            size_t bit_length) {
    std::ofstream out(path);
    Require(out.good(), "failed to open mock ciphertext material: " + path.string());
    for (size_t i = 0; i < bit_length; ++i) {
        out << (((value >> i) & 1U) != 0 ? 1 : 0) << "\n";
    }
}

void WriteManifest(const std::filesystem::path& path) {
    std::ofstream out(path);
    Require(out.good(), "failed to open fixed GC manifest: " + path.string());
    out << "# Fixed GC Demo Material\n\n"
        << "- Fixed start `a = " << kFixedStart << "` as mock `a'` bits.\n"
        << "- Fixed bound `b = " << kFixedBound << "` is compiled into `GC_f`.\n"
        << "- Runtime public input wires contain only `x_i`.\n"
        << "- `one' = Enc(1)` is represented as mock encrypted integer bits.\n"
#ifdef USE_EMP_GC
        << "- GC backend: EMP half-gates.\n";
#else
        << "- GC backend: in-repo minimal fallback.\n";
#endif
}

} // namespace

int main(int argc, char** argv) {
    const std::filesystem::path output_dir =
        argc > 1 ? std::filesystem::path(argv[1]) : std::filesystem::path("artifacts");
    std::filesystem::create_directories(output_dir);

    FHEContextWrapper fhe_ctx;
    FHEGates gates(fhe_ctx);
    ControlledRevealCircuit circuit_builder(fhe_ctx, gates);

    auto circuit = circuit_builder.DescribeFixedBoundLessOrEqualCircuit(
        kBitLength, kFixedBound);

    const auto circuit_dump = output_dir / "fixed_bound_circuit_g_demo.txt";
    const auto circuit_shape = output_dir / "fixed_bound_circuit_shape.bin";
    const auto artifact_path = output_dir / "fixed_bound_gc_artifact.bin";

    WriteBooleanCircuitText(circuit, circuit_dump.string(),
                            true /* redact_constant_values */);
    WriteBooleanCircuitShape(circuit, circuit_shape.string());

#ifdef USE_EMP_GC
    EmpGarbledCircuit garbler;
#else
    MinimalGarbledCircuit garbler;
#endif
    auto artifact = garbler.Garble(circuit);
    WriteActiveGarbledCircuitArtifact(artifact, artifact_path.string());

    WriteMockEncryptedBits(output_dir / "a_prime_mock_bits.txt",
                           kFixedStart, kBitLength);
    WriteMockEncryptedBits(output_dir / "one_prime_mock_bits.txt",
                           1, kBitLength);
    WriteMockEncryptedBits(output_dir / "fixed_b_prime_mock_bits.txt",
                           kFixedBound, kBitLength);
    WriteManifest(output_dir / "fixed_gc_manifest.md");

    std::cout << "Prepared fixed GC material in " << output_dir.string() << "\n";
    std::cout << "Circuit dump: " << circuit_dump.string() << "\n";
    std::cout << "Circuit shape: " << circuit_shape.string() << "\n";
    std::cout << "GC artifact: " << artifact_path.string() << "\n";
    std::cout << "Runtime public input wires: " << circuit.input_wires.size() << "\n";
    std::cout << "Fixed bound compiled into GC_f: " << kFixedBound << "\n";
    return 0;
}

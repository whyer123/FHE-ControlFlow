#include "src/gc/active_garbled_circuit_io.h"
#include "src/gc/boolean_circuit_io.h"
#include "src/gc/openfhe_lwe_int_material.h"
#ifdef USE_EMP_GC
#include "src/gc/emp_garbled_circuit.h"
#else
#include "src/gc/minimal_garbled_circuit.h"
#endif

#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

namespace {

using BitMap = std::unordered_map<WireId, bool>;

void Require(bool condition, const std::string& message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

std::unordered_map<std::string, WireId> WireNameMap(
    const BooleanCircuit& circuit) {
    std::unordered_map<std::string, WireId> map;
    for (const auto& wire : circuit.wires) {
        map.emplace(wire.name, wire.id);
    }
    return map;
}

void SetBits(BitMap& values,
             const std::unordered_map<std::string, WireId>& wires,
             const std::string& name,
             uint64_t value,
             size_t width) {
    for (size_t bit = 0; bit < width; ++bit) {
        const auto it = wires.find(name + "_bit_" + std::to_string(bit));
        Require(it != wires.end(), "missing circuit input wire: " + name);
        values[it->second] = ((value >> bit) & 1ULL) != 0;
    }
}

void SetCiphertext(BitMap& values,
                   const std::unordered_map<std::string, WireId>& wires,
                   const std::string& prefix,
                   const OpenFHELWEIntRuntimeCiphertextMaterial& ciphertext,
                   size_t modulus_bits) {
    for (size_t i = 0; i < ciphertext.a.size(); ++i) {
        SetBits(values, wires, prefix + "_a_" + std::to_string(i),
                ciphertext.a.at(i), modulus_bits);
    }
    SetBits(values, wires, prefix + "_body", ciphertext.body, modulus_bits);
}

BitMap BuildPredicateInputs(
    const BooleanCircuit& circuit,
    const OpenFHELWEIntRuntimeCiphertextMaterial& lhs,
    const OpenFHELWEIntRuntimeCiphertextMaterial& rhs,
    size_t modulus_bits) {
    const auto wires = WireNameMap(circuit);
    BitMap values;
    SetCiphertext(values, wires, "lhs", lhs, modulus_bits);
    SetCiphertext(values, wires, "rhs", rhs, modulus_bits);
    return values;
}

bool EvaluatePredicate(
    const BooleanCircuit& circuit,
    const ActiveGarbledCircuitArtifact& artifact,
    const OpenFHELWEIntRuntimeCiphertextMaterial& lhs,
    const OpenFHELWEIntRuntimeCiphertextMaterial& rhs,
    size_t modulus_bits) {
    const auto input_bits =
        BuildPredicateInputs(circuit, lhs, rhs, modulus_bits);

#ifdef USE_EMP_GC
    EmpGarbledCircuit gc;
    const auto outputs = gc.Evaluate(artifact, circuit, input_bits);
#else
    MinimalGarbledCircuit gc;
    const auto input_labels = gc.EncodeInputs(artifact, input_bits);
    const auto output_labels = gc.EvaluateLabels(artifact, input_labels);
    const auto outputs = gc.DecodeOutputs(artifact, output_labels);
#endif

    Require(outputs.size() == 1, "OpenFHE LWE integer GC must output one bit.");
    return outputs.front();
}

OpenFHELWEIntRuntimeCiphertextMaterial AddCiphertextsModQ(
    const OpenFHELWEIntRuntimeCiphertextMaterial& lhs,
    const OpenFHELWEIntRuntimeCiphertextMaterial& rhs) {
    Require(lhs.dimension == rhs.dimension,
            "ciphertext add dimension mismatch.");
    Require(lhs.ciphertext_modulus == rhs.ciphertext_modulus,
            "ciphertext add modulus mismatch.");
    Require(lhs.plaintext_modulus == rhs.plaintext_modulus,
            "ciphertext add plaintext modulus mismatch.");
    Require(lhs.a.size() == rhs.a.size(),
            "ciphertext add a-vector length mismatch.");

    OpenFHELWEIntRuntimeCiphertextMaterial out = lhs;
    for (size_t i = 0; i < out.a.size(); ++i) {
        out.a[i] = (lhs.a[i] + rhs.a[i]) % lhs.ciphertext_modulus;
    }
    out.body = (lhs.body + rhs.body) % lhs.ciphertext_modulus;
    return out;
}

std::string PredicateSequenceString(const std::vector<bool>& sequence) {
    std::ostringstream out;
    for (size_t i = 0; i < sequence.size(); ++i) {
        if (i != 0) {
            out << ",";
        }
        out << (sequence[i] ? 1 : 0);
    }
    return out.str();
}

} // namespace

int main(int argc, char** argv) {
    const auto material_dir =
        argc > 1 ? std::filesystem::path(argv[1])
                 : std::filesystem::path("artifacts/openfhe_lwe_int_runtime");

    const auto circuit_shape =
        material_dir / "openfhe_lwe_int_circuit_shape.bin";
    const auto artifact_path =
        material_dir / "openfhe_lwe_int_gc_artifact.bin";
    const auto runtime_material_path =
        material_dir / "openfhe_lwe_int_runtime_material.txt";

    const auto circuit = ReadBooleanCircuitShape(circuit_shape.string());
    const auto artifact =
        ReadActiveGarbledCircuitArtifact(artifact_path.string());
    const auto material =
        ReadOpenFHELWEIntRuntimeMaterial(runtime_material_path.string());
    const auto modulus_bits = material.ModulusBits();

    const size_t expected_inputs =
        2U * (material.a_prime.dimension + 1U) * modulus_bits;
    Require(circuit.input_bit_length == expected_inputs,
            "runtime circuit input length does not match material.");
    Require(circuit.input_wires.size() == expected_inputs,
            "runtime circuit must expose x' and b' ciphertext input bits.");

    auto state = material.a_prime;
    const auto bound = material.b_prime;
    const auto one = material.one_prime;

    std::vector<bool> predicate_sequence;
    size_t iterations = 0;
    constexpr size_t kMaxDemoIterations = 64;

    while (true) {
        const bool predicate =
            EvaluatePredicate(circuit, artifact, state, bound, modulus_bits);
        predicate_sequence.push_back(predicate);
        if (!predicate) {
            break;
        }

        ++iterations;
        Require(iterations <= kMaxDemoIterations,
                "runtime demo exceeded max iteration guard.");
        state = AddCiphertextsModQ(state, one);
    }

    std::cout << "--- OpenFHE LWE Integer GC Evaluator Runtime Demo ---\n";
    std::cout << "GC_f(x', b') = GC{ [Dec_hsk(x') <= Dec_hsk(b')] }\n";
    std::cout << "Evaluator runtime loaded only a', b', one', circuit shape, and GC artifact\n";
    std::cout << "Evaluator runtime did not load hpk or hsk\n";
    std::cout << "Runtime public input wires: "
              << circuit.input_wires.size() << "\n";
    std::cout << "Predicate sequence: "
              << PredicateSequenceString(predicate_sequence) << "\n";
    std::cout << "Encrypted loop iterations executed: " << iterations << "\n";
    return EXIT_SUCCESS;
}

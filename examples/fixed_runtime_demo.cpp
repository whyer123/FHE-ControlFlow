#ifndef MOCK_OPENFHE
#error "fixed_runtime_demo currently expects MOCK_OPENFHE ciphertext material."
#endif

#include "src/fhe/mock_openfhe.h"
#include "src/gc/active_garbled_circuit_io.h"
#include "src/gc/boolean_circuit_io.h"
#include "src/gc/garbled_predicate_evaluator.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

std::vector<LWECiphertext> ReadMockEncryptedBits(
    const std::filesystem::path& path,
    size_t expected_bit_length) {
    std::ifstream in(path);
    if (!in.good()) {
        throw std::runtime_error("failed to open mock ciphertext material: " +
                                 path.string());
    }

    std::vector<LWECiphertext> bits;
    int bit = 0;
    while (in >> bit) {
        if (bit != 0 && bit != 1) {
            throw std::runtime_error("mock ciphertext bit must be 0 or 1: " +
                                     path.string());
        }
        bits.emplace_back(bit != 0);
    }

    if (bits.size() != expected_bit_length) {
        throw std::runtime_error("mock ciphertext bit length mismatch: " +
                                 path.string());
    }
    return bits;
}

LWECiphertext MockXor(const LWECiphertext& lhs, const LWECiphertext& rhs) {
    return LWECiphertext(lhs.bit != rhs.bit);
}

LWECiphertext MockAnd(const LWECiphertext& lhs, const LWECiphertext& rhs) {
    return LWECiphertext(lhs.bit && rhs.bit);
}

std::vector<LWECiphertext> MockEncryptedAdd(
    const std::vector<LWECiphertext>& lhs,
    const std::vector<LWECiphertext>& rhs) {
    if (lhs.size() != rhs.size()) {
        throw std::invalid_argument("encrypted add expects equal bit widths.");
    }
    if (lhs.empty()) {
        return {};
    }

    std::vector<LWECiphertext> result(lhs.size());
    auto xor_bits = MockXor(lhs[0], rhs[0]);
    result[0] = xor_bits;
    auto carry = MockAnd(lhs[0], rhs[0]);

    for (size_t i = 1; i < lhs.size(); ++i) {
        xor_bits = MockXor(lhs[i], rhs[i]);
        result[i] = MockXor(xor_bits, carry);
        const auto generate = MockAnd(lhs[i], rhs[i]);
        const auto propagate = MockAnd(xor_bits, carry);
        carry = MockXor(generate, propagate);
    }

    return result;
}

} // namespace

int main(int argc, char** argv) {
    const std::filesystem::path material_dir =
        argc > 1 ? std::filesystem::path(argv[1]) : std::filesystem::path("artifacts");

    const auto circuit_shape =
        material_dir / "fixed_bound_circuit_shape.bin";
    const auto artifact_path =
        material_dir / "fixed_bound_gc_artifact.bin";

    auto circuit = ReadBooleanCircuitShape(circuit_shape.string());
    auto artifact = ReadActiveGarbledCircuitArtifact(artifact_path.string());

    if (circuit.input_wires.size() != circuit.input_bit_length) {
        throw std::runtime_error(
            "fixed runtime circuit must expose only x_i public inputs.");
    }

    auto state = ReadMockEncryptedBits(material_dir / "a_prime_mock_bits.txt",
                                       circuit.input_bit_length);
    const auto enc_one =
        ReadMockEncryptedBits(material_dir / "one_prime_mock_bits.txt",
                              circuit.input_bit_length);

    GarbledPredicateEvaluator predicate_gc(circuit, artifact);

    std::cout << "--- Fixed GC Evaluator Runtime Demo ---\n";
    std::cout << "Evaluator runtime input policy: fixed GC_f(x')\n";
    std::cout << "Evaluator runtime loaded only a', one', evaluation material, and fixed GC_f\n";
    std::cout << "Evaluator runtime did not load hpk or hsk\n";
    std::cout << "GC artifact name: "
              << predicate_gc.ArtifactInfo(circuit.input_bit_length).name << "\n";
    std::cout << "Runtime public input wires: "
              << circuit.input_wires.size() << "\n";

    size_t iterations = 0;
    while (true) {
        const bool predicate = predicate_gc.EvaluateFixedBound(state);
        std::cout << "GC_f(x') revealed predicate [x <= fixed_b] = "
                  << (predicate ? 1 : 0) << "\n";
        if (!predicate) {
            break;
        }

        ++iterations;
        state = MockEncryptedAdd(state, enc_one);
    }

    std::cout << "Encrypted loop iterations executed: " << iterations << "\n";
    std::cout << "Evaluator runtime completed without hpk, hsk, or free b' input.\n";
    return 0;
}

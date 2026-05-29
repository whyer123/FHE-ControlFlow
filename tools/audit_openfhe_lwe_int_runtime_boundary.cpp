#include "src/gc/active_garbled_circuit_io.h"
#include "src/gc/boolean_circuit_io.h"
#include "src/gc/openfhe_lwe_int_material.h"

#include <cstdlib>
#include <cctype>
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

std::string ReadFile(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary);
    Require(in.good(), "failed to open file for audit: " + path.string());
    std::ostringstream out;
    out << in.rdbuf();
    return out.str();
}

std::string Trim(const std::string& value) {
    size_t begin = 0;
    while (begin < value.size() &&
           std::isspace(static_cast<unsigned char>(value[begin]))) {
        ++begin;
    }

    size_t end = value.size();
    while (end > begin &&
           std::isspace(static_cast<unsigned char>(value[end - 1U]))) {
        --end;
    }
    return value.substr(begin, end - begin);
}

std::string RemoveWhitespace(const std::string& value) {
    std::string out;
    out.reserve(value.size());
    for (const auto ch : value) {
        if (!std::isspace(static_cast<unsigned char>(ch))) {
            out.push_back(ch);
        }
    }
    return out;
}

std::string ExtractFieldValue(const std::string& text,
                              const std::string& key) {
    std::istringstream in(text);
    std::string line;
    while (std::getline(in, line)) {
        const auto pos = line.find('=');
        if (pos == std::string::npos) {
            continue;
        }
        if (Trim(line.substr(0, pos)) == key) {
            return Trim(line.substr(pos + 1U));
        }
    }
    throw std::runtime_error("missing full material field: " + key);
}

std::string VectorLiteral(const std::vector<uint64_t>& values) {
    std::ostringstream out;
    out << "[";
    for (size_t i = 0; i < values.size(); ++i) {
        if (i != 0) {
            out << ",";
        }
        out << values[i];
    }
    out << "]";
    return out.str();
}

bool Contains(const std::string& haystack, const std::string& needle) {
    return haystack.find(needle) != std::string::npos;
}

void AuditNoSetupOnlyFieldStrings(const std::filesystem::path& runtime_dir,
                                  const std::vector<std::string>& hsk_literals) {
    for (const auto& entry :
         std::filesystem::recursive_directory_iterator(runtime_dir)) {
        if (!entry.is_regular_file()) {
            continue;
        }

        const auto filename = entry.path().filename().string();
        Require(!Contains(filename, "hsk_lwe_secret_key"),
                "runtime directory contains hsk key file name: " +
                    entry.path().string());
        Require(!Contains(filename, "hpk_lwe_public_key"),
                "runtime directory contains hpk key file name: " +
                    entry.path().string());

        const auto bytes = ReadFile(entry.path());
        Require(!Contains(bytes, "hsk.s_mod_q"),
                "runtime artifact contains hsk.s_mod_q field: " +
                    entry.path().string());
        Require(!Contains(bytes, "hsk.s_raw"),
                "runtime artifact contains hsk.s_raw field: " +
                    entry.path().string());
        Require(!Contains(bytes, "manual_dec"),
                "runtime artifact contains manual_dec field: " +
                    entry.path().string());
        Require(!Contains(bytes, ".plaintext="),
                "runtime artifact contains clear plaintext field: " +
                    entry.path().string());
        Require(!Contains(bytes, "hsk_lwe_secret_key"),
                "runtime artifact references hsk key file: " +
                    entry.path().string());
        Require(!Contains(bytes, "hpk_lwe_public_key"),
                "runtime artifact references hpk key file: " +
                    entry.path().string());
        for (const auto& literal : hsk_literals) {
            if (!literal.empty()) {
                Require(!Contains(bytes, literal),
                        "runtime artifact contains clear hsk vector: " +
                            entry.path().string());
            }
        }
    }
}

} // namespace

int main(int argc, char** argv) {
    Require(argc == 3,
            "usage: audit_openfhe_lwe_int_runtime_boundary "
            "<full-material.txt> <runtime-dir>");

    const auto full_material_path = std::filesystem::path(argv[1]);
    const auto runtime_dir = std::filesystem::path(argv[2]);
    const auto runtime_material_path =
        runtime_dir / "openfhe_lwe_int_runtime_material.txt";
    const auto circuit_shape_path =
        runtime_dir / "openfhe_lwe_int_circuit_shape.bin";
    const auto artifact_path =
        runtime_dir / "openfhe_lwe_int_gc_artifact.bin";

    Require(std::filesystem::is_regular_file(full_material_path),
            "full setup material file is missing.");
    Require(std::filesystem::is_directory(runtime_dir),
            "runtime directory is missing.");
    Require(std::filesystem::is_regular_file(runtime_material_path),
            "runtime material file is missing.");
    Require(std::filesystem::is_regular_file(circuit_shape_path),
            "runtime circuit shape is missing.");
    Require(std::filesystem::is_regular_file(artifact_path),
            "runtime GC artifact is missing.");

    const auto full_material =
        ReadOpenFHELWEIntMaterial(full_material_path.string());
    Require(!full_material.hsk_mod_q.empty(),
            "full setup material must contain hsk.s_mod_q.");
    const auto runtime_material =
        ReadOpenFHELWEIntRuntimeMaterial(runtime_material_path.string());
    const auto modulus_bits = runtime_material.ModulusBits();

    const auto full_text = ReadFile(full_material_path);
    Require(Contains(full_text, "hsk.s_mod_q"),
            "positive control failed: full material should contain hsk.s_mod_q.");
    Require(Contains(full_text, "manual_dec"),
            "positive control failed: full material should contain manual_dec.");
    const auto hsk_raw_literal = ExtractFieldValue(full_text, "hsk.s_raw");
    const auto hsk_mod_q_literal =
        ExtractFieldValue(full_text, "hsk.s_mod_q");

    const size_t expected_input_bits =
        2U * (runtime_material.a_prime.dimension + 1U) * modulus_bits;
    const auto circuit = ReadBooleanCircuitShape(circuit_shape_path.string());
    Require(circuit.input_wires.size() == expected_input_bits,
            "runtime circuit shape does not expose x'/b' ciphertext inputs.");
    Require(circuit.input_bit_length == expected_input_bits,
            "runtime circuit input length does not match material.");
    Require(circuit.secret_constant_wires.size() ==
                full_material.hsk_dimension * 4U,
            "runtime circuit shape has unexpected secret constant count.");
    for (const auto& secret : circuit.secret_constant_wires) {
        Require(!secret.second,
                "runtime circuit shape should not serialize secret values.");
    }

    const auto artifact =
        ReadActiveGarbledCircuitArtifact(artifact_path.string());
    Require(artifact.input_wires.size() == circuit.input_wires.size(),
            "GC artifact input wires do not match circuit shape.");
    Require(artifact.output_wires.size() == circuit.output_wires.size(),
            "GC artifact output wires do not match circuit shape.");
    Require(artifact.constant_labels.size() ==
                circuit.constant_wires.size() +
                    circuit.secret_constant_wires.size(),
            "GC artifact must contain selected labels for public and secret constants.");

    AuditNoSetupOnlyFieldStrings(
        runtime_dir,
        {hsk_raw_literal, RemoveWhitespace(hsk_raw_literal),
         hsk_mod_q_literal, RemoveWhitespace(hsk_mod_q_literal),
         VectorLiteral(full_material.hsk_mod_q)});

    std::cout << "OpenFHE LWE integer runtime boundary audit passed.\n";
    return EXIT_SUCCESS;
}

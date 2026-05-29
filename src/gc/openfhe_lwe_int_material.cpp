#include "openfhe_lwe_int_material.h"

#include <cctype>
#include <fstream>
#include <ostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>

namespace {

void Require(bool condition, const std::string& message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
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

uint64_t ParseU64(const std::unordered_map<std::string, std::string>& fields,
                  const std::string& key) {
    const auto it = fields.find(key);
    Require(it != fields.end(), "missing OpenFHE material field: " + key);
    return std::stoull(it->second);
}

std::vector<uint64_t> ParseVector(
    const std::unordered_map<std::string, std::string>& fields,
    const std::string& key) {
    const auto it = fields.find(key);
    Require(it != fields.end(), "missing OpenFHE material vector: " + key);
    std::string body = Trim(it->second);
    Require(body.size() >= 2 && body.front() == '[' && body.back() == ']',
            "invalid vector format for " + key);
    body = body.substr(1, body.size() - 2U);

    std::vector<uint64_t> out;
    std::stringstream stream(body);
    std::string item;
    while (std::getline(stream, item, ',')) {
        item = Trim(item);
        if (!item.empty()) {
            out.push_back(std::stoull(item));
        }
    }
    return out;
}

void WriteVector(std::ostream& out,
                 const std::string& key,
                 const std::vector<uint64_t>& values) {
    out << key << "=[";
    for (size_t i = 0; i < values.size(); ++i) {
        if (i != 0) {
            out << ",";
        }
        out << values[i];
    }
    out << "]\n";
}

size_t Log2PowerOfTwo(uint64_t value, const std::string& name) {
    Require(value > 0 && (value & (value - 1U)) == 0,
            name + " must be a power of two.");
    size_t bits = 0;
    while ((1ULL << bits) < value) {
        ++bits;
    }
    return bits;
}

OpenFHELWEIntCiphertextMaterial ParseCiphertext(
    const std::unordered_map<std::string, std::string>& fields,
    const std::string& prefix) {
    OpenFHELWEIntCiphertextMaterial out;
    out.plaintext = ParseU64(fields, prefix + ".plaintext");
    out.dimension =
        static_cast<size_t>(ParseU64(fields, prefix + ".dimension"));
    out.ciphertext_modulus =
        ParseU64(fields, prefix + ".ciphertext_modulus");
    out.plaintext_modulus =
        ParseU64(fields, prefix + ".plaintext_modulus");
    out.body = ParseU64(fields, prefix + ".body");
    out.a = ParseVector(fields, prefix + ".a");
    out.manual_dec = ParseU64(fields, prefix + ".manual_dec");

    Require(out.a.size() == out.dimension,
            prefix + " a-vector length does not match dimension.");
    Require(out.manual_dec == out.plaintext,
            prefix + " manual_dec does not match plaintext.");
    return out;
}

OpenFHELWEIntRuntimeCiphertextMaterial ToRuntimeCiphertext(
    const OpenFHELWEIntCiphertextMaterial& ciphertext) {
    OpenFHELWEIntRuntimeCiphertextMaterial out;
    out.dimension = ciphertext.dimension;
    out.ciphertext_modulus = ciphertext.ciphertext_modulus;
    out.plaintext_modulus = ciphertext.plaintext_modulus;
    out.body = ciphertext.body;
    out.a = ciphertext.a;
    return out;
}

OpenFHELWEIntRuntimeCiphertextMaterial ParseRuntimeCiphertext(
    const std::unordered_map<std::string, std::string>& fields,
    const std::string& prefix) {
    OpenFHELWEIntRuntimeCiphertextMaterial out;
    out.dimension =
        static_cast<size_t>(ParseU64(fields, prefix + ".dimension"));
    out.ciphertext_modulus =
        ParseU64(fields, prefix + ".ciphertext_modulus");
    out.plaintext_modulus =
        ParseU64(fields, prefix + ".plaintext_modulus");
    out.body = ParseU64(fields, prefix + ".body");
    out.a = ParseVector(fields, prefix + ".a");

    Require(out.a.size() == out.dimension,
            prefix + " a-vector length does not match dimension.");
    return out;
}

void WriteRuntimeCiphertext(
    std::ostream& out,
    const std::string& prefix,
    const OpenFHELWEIntRuntimeCiphertextMaterial& ciphertext) {
    out << prefix << ".dimension=" << ciphertext.dimension << "\n";
    out << prefix << ".ciphertext_modulus="
        << ciphertext.ciphertext_modulus << "\n";
    out << prefix << ".plaintext_modulus="
        << ciphertext.plaintext_modulus << "\n";
    out << prefix << ".body=" << ciphertext.body << "\n";
    WriteVector(out, prefix + ".a", ciphertext.a);
}

} // namespace

OpenFHELWEIntCircuitParams OpenFHELWEIntMaterial::CircuitParams() const {
    Require(a_prime.dimension == b_prime.dimension,
            "a'/b' dimensions do not match.");
    Require(a_prime.dimension == hsk_mod_q.size(),
            "ciphertext dimension does not match hsk.s_mod_q length.");
    Require(a_prime.ciphertext_modulus == b_prime.ciphertext_modulus,
            "a'/b' ciphertext moduli do not match.");
    Require(a_prime.plaintext_modulus == b_prime.plaintext_modulus,
            "a'/b' plaintext moduli do not match.");
    Require(a_prime.plaintext_modulus == plaintext_modulus,
            "top-level plaintext modulus does not match ciphertext.");
    Require(hsk_switched_modulus == a_prime.ciphertext_modulus,
            "hsk switched modulus does not match ciphertext modulus.");

    OpenFHELWEIntCircuitParams params;
    params.dimension = a_prime.dimension;
    params.ciphertext_modulus = a_prime.ciphertext_modulus;
    params.plaintext_modulus = plaintext_modulus;
    params.modulus_bits =
        Log2PowerOfTwo(params.ciphertext_modulus, "ciphertext modulus");
    params.plaintext_bits =
        Log2PowerOfTwo(params.plaintext_modulus, "plaintext modulus");
    Require(logical_plaintext_bits == params.plaintext_bits,
            "logical_plaintext_bits does not match plaintext modulus.");
    params.hsk_mod_q = hsk_mod_q;
    return params;
}

OpenFHELWEIntMaterial ReadOpenFHELWEIntMaterial(const std::string& path) {
    std::ifstream in(path);
    Require(in.good(), "failed to open OpenFHE LWE integer material: " + path);

    std::unordered_map<std::string, std::string> fields;
    std::string line;
    while (std::getline(in, line)) {
        line = Trim(line);
        if (line.empty() || line[0] == '#') {
            continue;
        }
        const auto pos = line.find('=');
        if (pos == std::string::npos) {
            continue;
        }
        fields.emplace(Trim(line.substr(0, pos)), Trim(line.substr(pos + 1U)));
    }

    const auto format_it = fields.find("format");
    Require(format_it != fields.end() &&
                format_it->second == "openfhe_lwe_int_setup_material_v1",
            "invalid OpenFHE LWE integer setup material format: " + path);

    OpenFHELWEIntMaterial material;
    material.logical_plaintext_bits =
        static_cast<size_t>(ParseU64(fields, "logical_plaintext_bits"));
    material.plaintext_modulus = ParseU64(fields, "plaintext_modulus");
    material.hsk_dimension =
        static_cast<size_t>(ParseU64(fields, "hsk.dimension"));
    material.hsk_modulus = ParseU64(fields, "hsk.modulus");
    material.hsk_switched_modulus = ParseU64(fields, "hsk.switched_modulus");
    material.hsk_mod_q = ParseVector(fields, "hsk.s_mod_q");
    material.a_prime = ParseCiphertext(fields, "a_prime");
    material.b_prime = ParseCiphertext(fields, "b_prime");
    material.one_prime = ParseCiphertext(fields, "one_prime");

    Require(material.hsk_mod_q.size() == material.hsk_dimension,
            "hsk.s_mod_q length does not match hsk.dimension.");
    (void)material.CircuitParams();
    return material;
}

size_t OpenFHELWEIntRuntimeMaterial::ModulusBits() const {
    Require(a_prime.dimension == b_prime.dimension,
            "runtime a'/b' dimensions do not match.");
    Require(a_prime.dimension == one_prime.dimension,
            "runtime one' dimension does not match a'.");
    Require(a_prime.ciphertext_modulus == b_prime.ciphertext_modulus,
            "runtime a'/b' ciphertext moduli do not match.");
    Require(a_prime.ciphertext_modulus == one_prime.ciphertext_modulus,
            "runtime one' ciphertext modulus does not match a'.");
    Require(a_prime.plaintext_modulus == b_prime.plaintext_modulus,
            "runtime a'/b' plaintext moduli do not match.");
    Require(a_prime.plaintext_modulus == one_prime.plaintext_modulus,
            "runtime one' plaintext modulus does not match a'.");
    Require(a_prime.plaintext_modulus == plaintext_modulus,
            "runtime top-level plaintext modulus does not match ciphertext.");
    Require(logical_plaintext_bits ==
                Log2PowerOfTwo(plaintext_modulus, "runtime plaintext modulus"),
            "runtime logical_plaintext_bits does not match plaintext modulus.");
    return Log2PowerOfTwo(a_prime.ciphertext_modulus,
                          "runtime ciphertext modulus");
}

OpenFHELWEIntRuntimeMaterial ToRuntimeMaterial(
    const OpenFHELWEIntMaterial& material) {
    (void)material.CircuitParams();
    OpenFHELWEIntRuntimeMaterial out;
    out.logical_plaintext_bits = material.logical_plaintext_bits;
    out.plaintext_modulus = material.plaintext_modulus;
    out.a_prime = ToRuntimeCiphertext(material.a_prime);
    out.b_prime = ToRuntimeCiphertext(material.b_prime);
    out.one_prime = ToRuntimeCiphertext(material.one_prime);
    (void)out.ModulusBits();
    return out;
}

void WriteOpenFHELWEIntRuntimeMaterial(
    const OpenFHELWEIntRuntimeMaterial& material,
    const std::string& path) {
    (void)material.ModulusBits();
    std::ofstream out(path);
    Require(out.good(), "failed to open OpenFHE runtime material: " + path);

    out << "format=openfhe_lwe_int_runtime_material_v1\n";
    out << "logical_plaintext_bits=" << material.logical_plaintext_bits << "\n";
    out << "plaintext_modulus=" << material.plaintext_modulus << "\n";
    WriteRuntimeCiphertext(out, "a_prime", material.a_prime);
    WriteRuntimeCiphertext(out, "b_prime", material.b_prime);
    WriteRuntimeCiphertext(out, "one_prime", material.one_prime);

    Require(static_cast<bool>(out),
            "failed to write OpenFHE runtime material: " + path);
}

OpenFHELWEIntRuntimeMaterial ReadOpenFHELWEIntRuntimeMaterial(
    const std::string& path) {
    std::ifstream in(path);
    Require(in.good(), "failed to open OpenFHE runtime material: " + path);

    std::unordered_map<std::string, std::string> fields;
    std::string line;
    while (std::getline(in, line)) {
        line = Trim(line);
        if (line.empty() || line[0] == '#') {
            continue;
        }
        const auto pos = line.find('=');
        if (pos == std::string::npos) {
            continue;
        }
        fields.emplace(Trim(line.substr(0, pos)), Trim(line.substr(pos + 1U)));
    }

    const auto format_it = fields.find("format");
    Require(format_it != fields.end() &&
                format_it->second == "openfhe_lwe_int_runtime_material_v1",
            "invalid OpenFHE runtime material format: " + path);

    OpenFHELWEIntRuntimeMaterial material;
    material.logical_plaintext_bits =
        static_cast<size_t>(ParseU64(fields, "logical_plaintext_bits"));
    material.plaintext_modulus = ParseU64(fields, "plaintext_modulus");
    material.a_prime = ParseRuntimeCiphertext(fields, "a_prime");
    material.b_prime = ParseRuntimeCiphertext(fields, "b_prime");
    material.one_prime = ParseRuntimeCiphertext(fields, "one_prime");
    (void)material.ModulusBits();
    return material;
}

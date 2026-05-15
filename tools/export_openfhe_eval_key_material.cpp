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

uint64_t ToU64(const NativeInteger& value) {
    return value.ConvertToInt();
}

class CppArrayWriter {
public:
    explicit CppArrayWriter(std::ostream& out) : out_(out) {}

    void Begin(const std::string& declaration) {
        out_ << declaration << " = {\n";
        column_ = 0;
        count_ = 0;
    }

    void Add(uint64_t value) {
        if (column_ == 0) {
            out_ << "    ";
        }
        out_ << "{" << value << "}";
        ++count_;
        ++column_;
        if (column_ == 6) {
            out_ << ",\n";
            column_ = 0;
        } else {
            out_ << ", ";
        }
    }

    uint64_t End() {
        if (column_ != 0) {
            out_ << "\n";
        }
        out_ << "};\n\n";
        return count_;
    }

private:
    std::ostream& out_;
    uint64_t column_ = 0;
    uint64_t count_ = 0;
};

uint64_t WriteNativeVector(CppArrayWriter& writer, const NativeVector& vector) {
    uint64_t words = 0;
    for (uint32_t i = 0; i < vector.GetLength(); ++i) {
        writer.Add(ToU64(vector[i]));
        ++words;
    }
    return words;
}

struct RefreshShape {
    size_t dim0 = 0;
    size_t dim1 = 0;
    size_t dim2 = 0;
    size_t rows = 0;
    size_t cols = 0;
    uint32_t poly_length = 0;
    uint64_t words = 0;
};

struct SwitchShape {
    size_t dim0 = 0;
    size_t dim1 = 0;
    size_t dim2 = 0;
    uint32_t vector_length = 0;
    uint64_t words_a = 0;
    uint64_t words_b = 0;
};

RefreshShape WriteRefreshKeyMaterial(std::ostream& out,
                                     const RingGSWACCKey& refresh_key) {
    Require(refresh_key != nullptr, "refresh key is null.");
    const auto& refresh = refresh_key->GetElements();

    RefreshShape shape = {};
    shape.dim0 = refresh.size();
    if (shape.dim0 > 0) {
        shape.dim1 = refresh[0].size();
    }
    if (shape.dim1 > 0) {
        shape.dim2 = refresh[0][0].size();
    }
    if (shape.dim2 > 0) {
        const auto& first_eval_key = refresh[0][0][0];
        Require(first_eval_key != nullptr, "first refresh eval key is null.");
        const auto& elements = first_eval_key->GetElements();
        shape.rows = elements.size();
        if (shape.rows > 0) {
            shape.cols = elements[0].size();
        }
        if (shape.cols > 0) {
            shape.poly_length = elements[0][0].GetValues().GetLength();
        }
    }

    out << "static const unsigned kFixedRefreshDim0 = " << shape.dim0 << ";\n";
    out << "static const unsigned kFixedRefreshDim1 = " << shape.dim1 << ";\n";
    out << "static const unsigned kFixedRefreshDim2 = " << shape.dim2 << ";\n";
    out << "static const unsigned kFixedRefreshRows = " << shape.rows << ";\n";
    out << "static const unsigned kFixedRefreshCols = " << shape.cols << ";\n";
    out << "static const unsigned kFixedRefreshPolyLength = "
        << shape.poly_length << ";\n\n";

    CppArrayWriter writer(out);
    writer.Begin("static const FixedRingGSWEvalKey FIXED_REFRESH_KEY[]");

    for (const auto& dim1 : refresh) {
        for (const auto& dim2 : dim1) {
            for (const auto& eval_key : dim2) {
                Require(eval_key != nullptr, "refresh eval key contains null.");
                for (const auto& row : eval_key->GetElements()) {
                    for (const auto& poly : row) {
                        shape.words +=
                            WriteNativeVector(writer, poly.GetValues());
                    }
                }
            }
        }
    }

    const uint64_t writer_words = writer.End();
    Require(writer_words == shape.words,
            "refresh writer count does not match measured word count.");
    out << "static const unsigned long long kFixedRefreshWordCount = "
        << shape.words << "ULL;\n\n";
    return shape;
}

SwitchShape WriteSwitchKeyMaterial(std::ostream& out,
                                   const LWESwitchingKey& switch_key) {
    Require(switch_key != nullptr, "switch key is null.");
    const auto& key_a = switch_key->GetElementsA();
    const auto& key_b = switch_key->GetElementsB();

    SwitchShape shape = {};
    shape.dim0 = key_a.size();
    if (shape.dim0 > 0) {
        shape.dim1 = key_a[0].size();
    }
    if (shape.dim1 > 0) {
        shape.dim2 = key_a[0][0].size();
    }
    if (shape.dim2 > 0) {
        shape.vector_length = key_a[0][0][0].GetLength();
    }

    Require(key_b.size() == shape.dim0,
            "switch key A/B dim0 mismatch.");

    out << "static const unsigned kFixedSwitchDim0 = " << shape.dim0 << ";\n";
    out << "static const unsigned kFixedSwitchDim1 = " << shape.dim1 << ";\n";
    out << "static const unsigned kFixedSwitchDim2 = " << shape.dim2 << ";\n";
    out << "static const unsigned kFixedSwitchVectorLength = "
        << shape.vector_length << ";\n\n";

    CppArrayWriter writer_a(out);
    writer_a.Begin("static const FixedLweSwitchingKey FIXED_SWITCH_KEY[]");

    for (const auto& dim1 : key_a) {
        for (const auto& dim2 : dim1) {
            for (const auto& vector : dim2) {
                shape.words_a += WriteNativeVector(writer_a, vector);
            }
        }
    }

    const uint64_t writer_words_a = writer_a.End();
    Require(writer_words_a == shape.words_a,
            "switch A writer count does not match measured word count.");

    CppArrayWriter writer_b(out);
    writer_b.Begin("static const FixedLweSwitchingKey FIXED_SWITCH_KEY_B[]");

    for (const auto& dim1 : key_b) {
        for (const auto& dim2 : dim1) {
            for (const auto& scalar : dim2) {
                writer_b.Add(ToU64(scalar));
                ++shape.words_b;
            }
        }
    }

    const uint64_t writer_words_b = writer_b.End();
    Require(writer_words_b == shape.words_b,
            "switch B writer count does not match measured word count.");
    out << "static const unsigned long long kFixedSwitchAWordCount = "
        << shape.words_a << "ULL;\n";
    out << "static const unsigned long long kFixedSwitchBWordCount = "
        << shape.words_b << "ULL;\n\n";
    return shape;
}

void WriteManifest(const std::filesystem::path& path,
                   const RefreshShape& refresh_shape,
                   const SwitchShape& switch_shape) {
    std::ofstream manifest(path);
    Require(manifest.good(), "failed to open manifest output.");

    manifest << "refresh.dim0=" << refresh_shape.dim0 << "\n";
    manifest << "refresh.dim1=" << refresh_shape.dim1 << "\n";
    manifest << "refresh.dim2=" << refresh_shape.dim2 << "\n";
    manifest << "refresh.rows=" << refresh_shape.rows << "\n";
    manifest << "refresh.cols=" << refresh_shape.cols << "\n";
    manifest << "refresh.poly_length=" << refresh_shape.poly_length << "\n";
    manifest << "refresh.words=" << refresh_shape.words << "\n";
    manifest << "switch.dim0=" << switch_shape.dim0 << "\n";
    manifest << "switch.dim1=" << switch_shape.dim1 << "\n";
    manifest << "switch.dim2=" << switch_shape.dim2 << "\n";
    manifest << "switch.vector_length=" << switch_shape.vector_length << "\n";
    manifest << "switch.words_a=" << switch_shape.words_a << "\n";
    manifest << "switch.words_b=" << switch_shape.words_b << "\n";
}

} // namespace

int main(int argc, char** argv) {
    const std::filesystem::path key_dir =
        argc > 1 ? std::filesystem::path(argv[1])
                 : std::filesystem::path("demo_keys/openfhe_binfhe_demo_keypair");
    const std::filesystem::path output_dir =
        argc > 2 ? std::filesystem::path(argv[2])
                 : std::filesystem::path("artifacts");

    std::filesystem::create_directories(output_dir);

    RingGSWACCKey refresh_key;
    LWESwitchingKey switch_key;

    Require(Serial::DeserializeFromFile(
                (key_dir / "eval_refresh_key.bin").string(), refresh_key,
                SerType::BINARY),
            "failed to deserialize refresh evaluation key.");
    Require(Serial::DeserializeFromFile(
                (key_dir / "eval_switch_key.bin").string(), switch_key,
                SerType::BINARY),
            "failed to deserialize switching evaluation key.");

    const auto material_path =
        output_dir / "openfhe_fixed_eval_key_material.cpp";
    const auto manifest_path =
        output_dir / "openfhe_fixed_eval_key_material_manifest.txt";

    std::ofstream material(material_path);
    Require(material.good(), "failed to open material output.");

    material << "// Generated fixed OpenFHE evaluation key material.\n";
    material << "// Source: " << key_dir.string() << "\n";
    material << "// This file intentionally contains plain integer arrays only.\n\n";
    material << "namespace controlled_reveal_reference_generated {\n\n";
    material << "typedef unsigned int u32;\n";
    material << "struct FixedRingGSWEvalKey { u32 value; };\n";
    material << "struct FixedLweSwitchingKey { u32 value; };\n\n";

    const RefreshShape refresh_shape =
        WriteRefreshKeyMaterial(material, refresh_key);
    const SwitchShape switch_shape =
        WriteSwitchKeyMaterial(material, switch_key);

    material << "} // namespace controlled_reveal_reference_generated\n";
    WriteManifest(manifest_path, refresh_shape, switch_shape);

    std::cout << "Exported OpenFHE eval key material to "
              << material_path.string() << "\n";
    std::cout << "Wrote material manifest to "
              << manifest_path.string() << "\n";
    return 0;
}

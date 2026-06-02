#include "active_garbled_circuit_io.h"

#include <algorithm>
#include <cstdint>
#include <fstream>
#include <stdexcept>
#include <utility>

namespace {

#ifdef USE_EMP_GC
constexpr const char* kMagic = "EMP_GC_ARTIFACT_V1";
#else
constexpr const char* kMagic = "MIN_GC_ARTIFACT_V1";
#endif

void Require(bool condition, const std::string& message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

void WriteU64(std::ostream& out, uint64_t value) {
    out.write(reinterpret_cast<const char*>(&value), sizeof(value));
}

uint64_t ReadU64(std::istream& in) {
    uint64_t value = 0;
    in.read(reinterpret_cast<char*>(&value), sizeof(value));
    Require(static_cast<bool>(in), "failed to read uint64 from GC artifact.");
    return value;
}

#ifndef USE_EMP_GC
void WriteBool(std::ostream& out, bool value) {
    const uint8_t byte = value ? 1 : 0;
    out.write(reinterpret_cast<const char*>(&byte), sizeof(byte));
}

bool ReadBool(std::istream& in) {
    uint8_t byte = 0;
    in.read(reinterpret_cast<char*>(&byte), sizeof(byte));
    Require(static_cast<bool>(in), "failed to read bool from GC artifact.");
    return byte != 0;
}
#endif

void WriteString(std::ostream& out, const std::string& value) {
    WriteU64(out, value.size());
    out.write(value.data(), static_cast<std::streamsize>(value.size()));
}

std::string ReadString(std::istream& in) {
    const auto size = ReadU64(in);
    std::string value(size, '\0');
    in.read(value.data(), static_cast<std::streamsize>(size));
    Require(static_cast<bool>(in), "failed to read string from GC artifact.");
    return value;
}

void WriteWireVector(std::ostream& out, const std::vector<WireId>& wires) {
    WriteU64(out, wires.size());
    for (const auto wire : wires) {
        WriteU64(out, wire);
    }
}

std::vector<WireId> ReadWireVector(std::istream& in) {
    const auto size = ReadU64(in);
    std::vector<WireId> wires;
    wires.reserve(size);
    for (uint64_t i = 0; i < size; ++i) {
        wires.push_back(static_cast<WireId>(ReadU64(in)));
    }
    return wires;
}

template <typename Map>
std::vector<WireId> SortedKeys(const Map& map) {
    std::vector<WireId> keys;
    keys.reserve(map.size());
    for (const auto& item : map) {
        keys.push_back(item.first);
    }
    std::sort(keys.begin(), keys.end());
    return keys;
}

#ifdef USE_EMP_GC
void WriteBlock(std::ostream& out, const emp::block& block) {
    out.write(reinterpret_cast<const char*>(&block), sizeof(block));
}

emp::block ReadBlock(std::istream& in) {
    emp::block block;
    in.read(reinterpret_cast<char*>(&block), sizeof(block));
    Require(static_cast<bool>(in), "failed to read EMP block from GC artifact.");
    return block;
}
#endif

} // namespace

void WriteActiveGarbledCircuitArtifact(
    const ActiveGarbledCircuitArtifact& artifact,
    const std::string& path) {
    std::ofstream out(path, std::ios::binary);
    Require(out.good(), "failed to open GC artifact for writing: " + path);

    WriteString(out, kMagic);
    WriteString(out, artifact.name);
    WriteWireVector(out, artifact.input_wires);
    WriteWireVector(out, artifact.output_wires);

#ifdef USE_EMP_GC
    WriteU64(out, artifact.input_bit_length);
    WriteU64(out, artifact.gate_count);
    WriteU64(out, artifact.and_gate_count);
    WriteBlock(out, artifact.delta);

    const auto input_keys = SortedKeys(artifact.public_input_labels);
    WriteU64(out, input_keys.size());
    for (const auto wire : input_keys) {
        WriteU64(out, wire);
        const auto& labels = artifact.public_input_labels.at(wire).labels;
        WriteBlock(out, labels[0]);
        WriteBlock(out, labels[1]);
    }

    const auto constant_keys = SortedKeys(artifact.constant_labels);
    WriteU64(out, constant_keys.size());
    for (const auto wire : constant_keys) {
        WriteU64(out, wire);
        WriteBlock(out, artifact.constant_labels.at(wire));
    }

    const auto output_keys = SortedKeys(artifact.output_zero_labels);
    WriteU64(out, output_keys.size());
    for (const auto wire : output_keys) {
        WriteU64(out, wire);
        WriteBlock(out, artifact.output_zero_labels.at(wire));
    }

    WriteU64(out, artifact.transcript.size());
    for (const auto& block : artifact.transcript) {
        WriteBlock(out, block);
    }
#else
    const auto constant_keys = SortedKeys(artifact.constant_labels);
    WriteU64(out, constant_keys.size());
    for (const auto wire : constant_keys) {
        WriteU64(out, wire);
        WriteString(out, artifact.constant_labels.at(wire));
    }

    WriteU64(out, artifact.gates.size());
    for (const auto& gate : artifact.gates) {
        WriteU64(out, gate.id);
        WriteU64(out, static_cast<uint64_t>(gate.kind));
        WriteWireVector(out, gate.inputs);
        WriteU64(out, gate.output);
        WriteU64(out, gate.table.size());
        for (const auto& entry : gate.table) {
            WriteString(out, entry.encrypted_label);
            WriteString(out, entry.tag);
        }
    }

    const auto input_keys = SortedKeys(artifact.public_input_labels);
    WriteU64(out, input_keys.size());
    for (const auto wire : input_keys) {
        WriteU64(out, wire);
        const auto& labels = artifact.public_input_labels.at(wire).labels;
        WriteString(out, labels[0]);
        WriteString(out, labels[1]);
    }

    const auto output_keys = SortedKeys(artifact.output_decoding);
    WriteU64(out, output_keys.size());
    for (const auto wire : output_keys) {
        WriteU64(out, wire);
        const auto& decoding = artifact.output_decoding.at(wire);
        WriteU64(out, decoding.size());
        std::vector<std::pair<std::string, bool>> rows(decoding.begin(),
                                                       decoding.end());
        std::sort(rows.begin(), rows.end(),
                  [](const auto& lhs, const auto& rhs) {
                      return lhs.first < rhs.first;
                  });
        for (const auto& row : rows) {
            WriteString(out, row.first);
            WriteBool(out, row.second);
        }
    }
#endif

    Require(static_cast<bool>(out), "failed to write GC artifact: " + path);
}

ActiveGarbledCircuitArtifact ReadActiveGarbledCircuitArtifact(
    const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    Require(in.good(), "failed to open GC artifact for reading: " + path);

    const auto magic = ReadString(in);
    Require(magic == kMagic, "GC artifact backend does not match runtime: " + path);

    ActiveGarbledCircuitArtifact artifact;
    artifact.name = ReadString(in);
    artifact.input_wires = ReadWireVector(in);
    artifact.output_wires = ReadWireVector(in);

#ifdef USE_EMP_GC
    artifact.input_bit_length = static_cast<size_t>(ReadU64(in));
    artifact.gate_count = static_cast<size_t>(ReadU64(in));
    artifact.and_gate_count = static_cast<size_t>(ReadU64(in));
    artifact.delta = ReadBlock(in);

    const auto input_count = ReadU64(in);
    for (uint64_t i = 0; i < input_count; ++i) {
        const auto wire = static_cast<WireId>(ReadU64(in));
        EmpWireLabels labels;
        labels.labels[0] = ReadBlock(in);
        labels.labels[1] = ReadBlock(in);
        artifact.public_input_labels.emplace(wire, labels);
    }

    const auto constant_count = ReadU64(in);
    for (uint64_t i = 0; i < constant_count; ++i) {
        artifact.constant_labels.emplace(
            static_cast<WireId>(ReadU64(in)), ReadBlock(in));
    }

    const auto output_count = ReadU64(in);
    for (uint64_t i = 0; i < output_count; ++i) {
        artifact.output_zero_labels.emplace(
            static_cast<WireId>(ReadU64(in)), ReadBlock(in));
    }

    const auto transcript_count = ReadU64(in);
    artifact.transcript.reserve(transcript_count);
    for (uint64_t i = 0; i < transcript_count; ++i) {
        artifact.transcript.push_back(ReadBlock(in));
    }
#else
    const auto constant_count = ReadU64(in);
    for (uint64_t i = 0; i < constant_count; ++i) {
        artifact.constant_labels.emplace(
            static_cast<WireId>(ReadU64(in)), ReadString(in));
    }

    const auto gate_count = ReadU64(in);
    artifact.gates.reserve(gate_count);
    for (uint64_t i = 0; i < gate_count; ++i) {
        GarbledGate gate;
        gate.id = static_cast<GateId>(ReadU64(in));
        gate.kind = static_cast<BitGateKind>(ReadU64(in));
        gate.inputs = ReadWireVector(in);
        gate.output = static_cast<WireId>(ReadU64(in));
        const auto row_count = ReadU64(in);
        gate.table.reserve(row_count);
        for (uint64_t row = 0; row < row_count; ++row) {
            gate.table.push_back({ReadString(in), ReadString(in)});
        }
        artifact.gates.push_back(std::move(gate));
    }

    const auto input_count = ReadU64(in);
    for (uint64_t i = 0; i < input_count; ++i) {
        const auto wire = static_cast<WireId>(ReadU64(in));
        artifact.public_input_labels.emplace(
            wire, WireLabels{{ReadString(in), ReadString(in)}});
    }

    const auto output_count = ReadU64(in);
    for (uint64_t i = 0; i < output_count; ++i) {
        const auto wire = static_cast<WireId>(ReadU64(in));
        const auto row_count = ReadU64(in);
        for (uint64_t row = 0; row < row_count; ++row) {
            artifact.output_decoding[wire].emplace(ReadString(in),
                                                   ReadBool(in));
        }
    }
#endif

    return artifact;
}

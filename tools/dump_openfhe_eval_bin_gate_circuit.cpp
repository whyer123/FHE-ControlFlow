#include "src/gc/boolean_circuit_export.h"
#include "src/gc/openfhe_eval_bin_gate_circuit.h"

#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

struct GateDumpSpec {
    OpenFHEBinGateKind gate;
    std::string name;
};

std::vector<GateDumpSpec> GateSpecs() {
    return {
        {OpenFHEBinGateKind::And, "and"},
        {OpenFHEBinGateKind::Or, "or"},
        {OpenFHEBinGateKind::Xor, "xor"},
        {OpenFHEBinGateKind::Xnor, "xnor"},
    };
}

} // namespace

int main(int argc, char** argv) {
    try {
        const std::filesystem::path output_dir =
            argc > 1 ? std::filesystem::path(argv[1])
                     : std::filesystem::path("artifacts");
        std::filesystem::create_directories(output_dir);

        std::cout << "Dumped OpenFHE EvalBinGate Boolean circuits to "
                  << output_dir.string() << "\n";
        std::cout << "bootstrap core status: placeholder\n";

        for (const auto& spec : GateSpecs()) {
            const auto circuit =
                OpenFHEEvalBinGateCircuit::DescribeDemoEvalBinGate(spec.gate);
            const auto path =
                output_dir /
                ("openfhe_evalbingate_" + spec.name + "_demo.txt");
            WriteBooleanCircuitText(circuit, path.string(), true);
            std::cout << spec.name << ": " << circuit.gates.size()
                      << " gates, " << circuit.wires.size()
                      << " wires, dump=" << path.string() << "\n";
        }
    } catch (const std::exception& ex) {
        std::cerr << "dump_openfhe_eval_bin_gate_circuit failed: "
                  << ex.what() << "\n";
        return 1;
    }

    return 0;
}

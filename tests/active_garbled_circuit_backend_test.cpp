#include "src/gc/active_garbled_circuit.h"

#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <unordered_map>
#include <vector>

namespace {

void Require(bool condition, const std::string& message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

BooleanCircuit BuildTestCircuit() {
    BooleanCircuitBuilder builder("active_backend_smoke", 2);
    const auto lhs = builder.AddInputWire("lhs");
    const auto rhs = builder.AddInputWire("rhs");
    const auto secret_true = builder.AddSecretConstantWire("secret_true", true);
    const auto both = builder.AddGate(BitGateKind::And, {lhs, rhs}, "both");
    const auto masked =
        builder.AddGate(BitGateKind::Xor, {both, secret_true}, "masked");
    builder.AddOutputWire(masked);
    return builder.Build();
}

std::vector<bool> Evaluate(const BooleanCircuit& circuit, bool lhs, bool rhs) {
    const auto artifact = GarbleActiveCircuit(circuit);
    return EvaluateActiveCircuit(
        artifact, circuit,
        {{circuit.input_wires.at(0), lhs}, {circuit.input_wires.at(1), rhs}});
}

} // namespace

int main() {
    try {
        const auto circuit = BuildTestCircuit();
        Require(ActiveGarbledCircuitBackendName() != "",
                "active backend must report a non-empty name");

        Require(Evaluate(circuit, false, false) == std::vector<bool>{true},
                "active backend expected !(0 & 0)");
        Require(Evaluate(circuit, false, true) == std::vector<bool>{true},
                "active backend expected !(0 & 1)");
        Require(Evaluate(circuit, true, false) == std::vector<bool>{true},
                "active backend expected !(1 & 0)");
        Require(Evaluate(circuit, true, true) == std::vector<bool>{false},
                "active backend expected !(1 & 1)");

        std::cout << "Active garbled circuit backend test passed with "
                  << ActiveGarbledCircuitBackendName() << ".\n";
        return EXIT_SUCCESS;
    } catch (const std::exception& error) {
        std::cerr << error.what() << "\n";
        return EXIT_FAILURE;
    }
}

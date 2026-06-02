#include "active_garbled_circuit.h"

std::string ActiveGarbledCircuitBackendName() {
#ifdef USE_EMP_GC
    return "EMP half-gates";
#else
    return "in-repo minimal fallback";
#endif
}

ActiveGarbledCircuitArtifact GarbleActiveCircuit(
    const BooleanCircuit& circuit) {
#ifdef USE_EMP_GC
    EmpGarbledCircuit gc;
    return gc.Garble(circuit);
#else
    MinimalGarbledCircuit gc;
    return gc.Garble(circuit);
#endif
}

std::vector<bool> EvaluateActiveCircuit(
    const ActiveGarbledCircuitArtifact& artifact,
    const BooleanCircuit& circuit,
    const std::unordered_map<WireId, bool>& input_bits) {
#ifdef USE_EMP_GC
    EmpGarbledCircuit gc;
    return gc.Evaluate(artifact, circuit, input_bits);
#else
    (void)circuit;
    MinimalGarbledCircuit gc;
    const auto input_labels = gc.EncodeInputs(artifact, input_bits);
    const auto output_labels = gc.EvaluateLabels(artifact, input_labels);
    return gc.DecodeOutputs(artifact, output_labels);
#endif
}

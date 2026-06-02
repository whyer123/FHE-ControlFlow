#pragma once

#ifdef USE_EMP_GC
#include "src/gc/emp_garbled_circuit.h"
using ActiveGarbledCircuitArtifact = EmpGarbledCircuitArtifact;
#else
#include "src/gc/minimal_garbled_circuit.h"
using ActiveGarbledCircuitArtifact = GarbledCircuitArtifact;
#endif

#include <string>
#include <unordered_map>
#include <vector>

std::string ActiveGarbledCircuitBackendName();

ActiveGarbledCircuitArtifact GarbleActiveCircuit(
    const BooleanCircuit& circuit);

std::vector<bool> EvaluateActiveCircuit(
    const ActiveGarbledCircuitArtifact& artifact,
    const BooleanCircuit& circuit,
    const std::unordered_map<WireId, bool>& input_bits);

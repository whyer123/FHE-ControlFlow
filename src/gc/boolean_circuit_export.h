#pragma once

#include "src/gc/boolean_circuit.h"
#include <string>

std::string BooleanCircuitToText(const BooleanCircuit& circuit);
void WriteBooleanCircuitText(const BooleanCircuit& circuit,
                             const std::string& path);

#pragma once

#include "src/gc/boolean_circuit.h"
#include <string>

std::string BooleanCircuitToText(const BooleanCircuit& circuit,
                                 bool redact_constant_values = false);
void WriteBooleanCircuitText(const BooleanCircuit& circuit,
                             const std::string& path,
                             bool redact_constant_values = false);

#pragma once

#include "src/gc/boolean_circuit.h"
#include <string>

void WriteBooleanCircuitShape(const BooleanCircuit& circuit,
                              const std::string& path);
BooleanCircuit ReadBooleanCircuitShape(const std::string& path);

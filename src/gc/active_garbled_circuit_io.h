#pragma once

#include "src/gc/active_garbled_circuit.h"
#include <string>

void WriteActiveGarbledCircuitArtifact(
    const ActiveGarbledCircuitArtifact& artifact,
    const std::string& path);

ActiveGarbledCircuitArtifact ReadActiveGarbledCircuitArtifact(
    const std::string& path);

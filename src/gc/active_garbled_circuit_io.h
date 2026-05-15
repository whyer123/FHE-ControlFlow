#pragma once

#include "src/gc/garbled_predicate_evaluator.h"
#include <string>

void WriteActiveGarbledCircuitArtifact(
    const ActiveGarbledCircuitArtifact& artifact,
    const std::string& path);

ActiveGarbledCircuitArtifact ReadActiveGarbledCircuitArtifact(
    const std::string& path);

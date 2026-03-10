#pragma once

#include "src/fhe/fhe_context.h"
#include "src/gates/fhe_gates.h"
#include "src/algorithms/fhe_cmp.h"
#include "src/selector/selector_api.h"

class LoopController {
public:
    LoopController(FHEContextWrapper& ctx, FHEGates& gates, FHECompare& cmp, SelectorAPI& selector)
        : fhe_ctx(ctx), fhe_gates(gates), fhe_cmp(cmp), sel(selector) {}

    // Decrements a binary integer
    std::vector<LWECiphertext> Decrement(const std::vector<LWECiphertext>& x);

    // Executes a simple while (x > 0) { x = x - 1; } loop
    void RunWhileLoop(std::vector<LWECiphertext> x);

private:
    FHEContextWrapper& fhe_ctx;
    FHEGates& fhe_gates;
    FHECompare& fhe_cmp;
    SelectorAPI& sel;
};

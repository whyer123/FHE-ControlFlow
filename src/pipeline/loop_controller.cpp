#include "loop_controller.h"
#include <iostream>

std::vector<LWECiphertext> LoopController::Decrement(const std::vector<LWECiphertext>& x) {
    if (x.empty()) return {};
    std::vector<LWECiphertext> result(x.size());
    std::vector<LWECiphertext> borrow(x.size());
    
    // bit 0
    result[0] = fhe_gates.EvalNOT(x[0]);
    borrow[0] = fhe_gates.EvalNOT(x[0]);
    
    for (size_t i = 1; i < x.size(); ++i) {
        result[i] = fhe_gates.EvalXOR(x[i], borrow[i-1]);
        auto not_xi = fhe_gates.EvalNOT(x[i]);
        borrow[i] = fhe_gates.EvalAND(not_xi, borrow[i-1]);
    }
    return result;
}

void LoopController::RunWhileLoop(std::vector<LWECiphertext> x) {
    auto zero = fhe_ctx.EncryptInteger(0, x.size());
    int iter_count = 0;
    
    std::cout << "[LoopController] Starting while (x > 0) loop..." << std::endl;
    while (true) {
        // Evaluate condition: cond = (x > 0)
        auto cond_bit = fhe_cmp.GreaterThan(x, zero);
        
        // Extract condition bit using Trusted Selector
        bool should_continue = sel.ExtractConditionBit(cond_bit);
        
        if (!should_continue) {
            std::cout << "[LoopController] Loop terminated. condition = false." << std::endl;
            break;
        }
        
        iter_count++;
        std::cout << "[LoopController] Iteration " << iter_count << " executed." << std::endl;
        
        // Decrement x
        x = Decrement(x);
    }
    
    std::cout << "[LoopController] Total iterations performed: " << iter_count << std::endl;
}

#include "loop_controller.h"
#include <iostream>

std::vector<LWECiphertext> LoopController::Decrement(const std::vector<LWECiphertext>& x) {
    if (x.empty()) return {};
    std::vector<LWECiphertext> result(x.size());
    std::vector<LWECiphertext> borrow(x.size());
    
    // bit 0 (x - 1)
    // result[0] = x[0] XOR 1 = NOT(x[0])
    result[0] = fhe_gates.EvalNOT(x[0]);
    // borrow[1] = NOT(x[0]) AND 1 = NOT(x[0])
    borrow[0] = fhe_gates.EvalNOT(x[0]);
    
    for (size_t i = 1; i < x.size(); ++i) {
        // result[i] = x[i] XOR borrow[i]
        result[i] = fhe_gates.EvalXOR(x[i], borrow[i-1]);
        // new_borrow = NOT(x[i]) AND borrow[i]
        auto not_xi = fhe_gates.EvalNOT(x[i]);
        borrow[i] = fhe_gates.EvalAND(not_xi, borrow[i-1]);
    }
    return result;
}

std::vector<LWECiphertext> LoopController::RunWhileLoop(
    std::vector<LWECiphertext> x, const std::vector<LWECiphertext>& target) {
    int iter_count = 0;
    auto state_token = sel.BootstrapToken();
    
    std::cout << "[LoopController] Starting ABE2-driven while (x > target) loop..." << std::endl;
    while (!sel.IsTerminal(state_token)) {
        const auto node_kind = sel.GetNodeKind(state_token);
        const auto state_name = sel.DescribeState(state_token);

        if (node_kind == ABE2NodeKind::Conditional) {
            auto cond_bit = fhe_cmp.GreaterThan(x, target);
            auto transition = sel.EvaluateConditional(state_token, cond_bit);
            std::cout << "[LoopController] " << state_name << " -> "
                      << sel.DescribeState(transition.next_token) << std::endl;
            state_token = transition.next_token;
            continue;
        }

        if (node_kind == ABE2NodeKind::Work) {
            iter_count++;
            std::cout << "[LoopController] Iteration " << iter_count << " executed." << std::endl;
            x = Decrement(x);
            state_token = sel.EvaluateUnconditional(state_token).next_token;
            continue;
        }

        break;
    }
    
    std::cout << "[LoopController] Total iterations performed: " << iter_count << std::endl;
    std::cout << "[LoopController] Final encrypted state reached target ciphertext domain." << std::endl;
    return x;
}

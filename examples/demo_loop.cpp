#include <iostream>
#include "src/fhe/fhe_context.h"
#include "src/gates/fhe_gates.h"
#include "src/algorithms/fhe_cmp.h"
#include "src/selector/abe_selector.cpp"
#include "src/pipeline/loop_controller.h"

int main() {
    std::cout << "--- FHE Control Flow Prototype (ABE GKP13 Single-Party) ---" << std::endl;
    
    // 1. Initialize FHE Context
    std::cout << "Initializing Context and Keys..." << std::endl;
    FHEContextWrapper fhe_ctx;
    
    // 2. Initialize Gates and Algorithms
    FHEGates gates(fhe_ctx);
    FHECompare cmp(gates);
    
    // 3. Initialize ABE Selector (Single-Party Simulation)
    ABESelector selector(fhe_ctx);
    
    // 4. Initialize Pipeline
    LoopController loop(fhe_ctx, gates, cmp, selector);
    
    // Input value
    int64_t input_val = 3;
    size_t bit_length = 4; // Use small bit length for speed prototyping
    
    std::cout << "Encrypting input x = " << input_val << " (bit_length = " << bit_length << ")" << std::endl;
    auto enc_x = fhe_ctx.EncryptInteger(input_val, bit_length);
    
    // 5. Run Demo
    loop.RunWhileLoop(enc_x);
    
    std::cout << "Demo completed successfully." << std::endl;
    return 0;
}

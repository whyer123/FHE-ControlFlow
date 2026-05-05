#include <iostream>
#include "src/fhe/fhe_context.h"
#include "src/gates/fhe_gates.h"
#include "src/gc/controlled_reveal_circuit.h"
#include "src/gc/predicate_gc.h"

int main() {
    std::cout << "--- Controlled Reveal Predicate Prototype ---" << std::endl;
    
    // 1. Initialize FHE Context
    std::cout << "Initializing Context and Keys..." << std::endl;
    FHEContextWrapper fhe_ctx;
    
    // 2. Initialize bit-level gates
    FHEGates gates(fhe_ctx);
    
    // 3. Build Algorithm 0: g(c_x,c_b)=Dec(Eval([x<=b],c_x,c_b)).
    ControlledRevealCircuit gc_f(fhe_ctx, gates);
    EncryptedPredicateEvaluator& predicate_gc = gc_f;
    
    // Encrypted endpoints for the loop: start at a' and stop at b'.
    int64_t start_value = 3;
    int64_t target_value = 7;
    size_t bit_length = 4; // Use small bit length for speed prototyping
    
    std::cout << "Encrypting loop endpoints a' and b' (bit_length = " << bit_length << ")" << std::endl;
    auto enc_a = fhe_ctx.EncryptInteger(start_value, bit_length);
    auto enc_b = fhe_ctx.EncryptInteger(target_value, bit_length);

    auto circuit = gc_f.DescribeLessOrEqualCircuit(bit_length);
    auto artifact = predicate_gc.ArtifactInfo(bit_length);
    std::cout << "GC artifact name: " << artifact.name << std::endl;
    std::cout << "GC artifact gate count: " << artifact.gate_count << std::endl;
    std::cout << "Circuit_g gates:" << std::endl;
    for (const auto& gate : circuit.gates) {
        std::cout << "  " << gate.output << " <- "
                  << BitGateKindToString(gate.kind) << "(";
        for (size_t i = 0; i < gate.inputs.size(); ++i) {
            if (i > 0) {
                std::cout << ", ";
            }
            std::cout << gate.inputs[i];
        }
        std::cout << ")" << std::endl;
    }
    
    // 4. Run Algorithm 0 for the encrypted predicate only.
    bool predicate = predicate_gc.Evaluate(enc_a, enc_b);
    
    std::cout << "GC_f(a', b') revealed predicate [a <= b] = "
              << (predicate ? 1 : 0) << std::endl;
    std::cout << "Demo completed without decrypting a, b, or the encrypted state." << std::endl;
    return 0;
}

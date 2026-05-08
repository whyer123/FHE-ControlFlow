#include <iostream>
#include "src/algorithms/fhe_arithmetic.h"
#include "src/fhe/fhe_context.h"
#include "src/gates/fhe_gates.h"
#include "src/gc/controlled_reveal_circuit.h"
#ifdef USE_EMP_GC
#include "src/gc/emp_garbled_circuit.h"
#else
#include "src/gc/minimal_garbled_circuit.h"
#endif
#include "src/gc/garbled_predicate_evaluator.h"
#include "src/gc/predicate_gc.h"

int main() {
    std::cout << "--- Controlled Reveal Predicate Prototype ---" << std::endl;
    
    // 1. Initialize FHE Context
    std::cout << "Initializing Context and Keys..." << std::endl;
    FHEContextWrapper fhe_ctx;
    
    // 2. Initialize bit-level gates
    FHEGates gates(fhe_ctx);
    FHEArithmetic arithmetic(gates);
    
    // 3. Build Algorithm 0: g(c_x,c_b)=Dec(Eval([x<=b],c_x,c_b)).
    ControlledRevealCircuit circuit_builder(fhe_ctx, gates);
    
    // Encrypted endpoints for the loop: start at a' and stop after b'.
    int64_t start_value = 3;
    int64_t target_value = 7;
    size_t bit_length = 4; // Use small bit length for speed prototyping
    
    std::cout << "Encrypting loop endpoints a' and b' (bit_length = " << bit_length << ")" << std::endl;
    auto enc_a = fhe_ctx.EncryptInteger(start_value, bit_length);
    auto enc_b = fhe_ctx.EncryptInteger(target_value, bit_length);

    auto circuit = circuit_builder.DescribeLessOrEqualCircuit(bit_length);
#ifdef USE_EMP_GC
    EmpGarbledCircuit garbler;
    auto garbled_artifact = garbler.Garble(circuit);
#else
    MinimalGarbledCircuit garbler;
    auto garbled_artifact = garbler.Garble(circuit);
#endif
    GarbledPredicateEvaluator garbled_predicate(circuit, garbled_artifact);
#ifdef MOCK_OPENFHE
    EncryptedPredicateEvaluator& predicate_gc = garbled_predicate;
#ifdef USE_EMP_GC
    std::cout << "Evaluator predicate path: EMP half-gates GC artifact" << std::endl;
#else
    std::cout << "Evaluator predicate path: Minimal GC artifact" << std::endl;
#endif
#else
    EncryptedPredicateEvaluator& predicate_gc = circuit_builder;
    std::cout << "Evaluator predicate path: direct OpenFHE controlled reveal fallback" << std::endl;
#endif
    auto artifact = predicate_gc.ArtifactInfo(bit_length);
    std::cout << "GC artifact name: " << artifact.name << std::endl;
    std::cout << "GC artifact gate count: " << artifact.gate_count << std::endl;
    std::cout << "Public input label pairs: "
              << garbled_artifact.public_input_labels.size() << std::endl;
    std::cout << "Hardcoded secret/constant labels: "
              << garbled_artifact.constant_labels.size() << std::endl;
    std::cout << "Circuit_g constant wires: "
              << circuit.constant_wires.size() << std::endl;
#ifdef USE_EMP_GC
    std::cout << "EMP half-gates AND count: "
              << garbled_artifact.and_gate_count << std::endl;
    std::cout << "EMP transcript blocks: "
              << garbled_artifact.transcript.size() << std::endl;
#endif
    std::cout << "Circuit_g gates:" << std::endl;
    for (const auto& gate : circuit.gates) {
        std::cout << "  g" << gate.id << ": w" << gate.output
                  << "(" << circuit.Wire(gate.output).name << ") <- "
                  << BitGateKindToString(gate.kind) << "(";
        for (size_t i = 0; i < gate.inputs.size(); ++i) {
            if (i > 0) {
                std::cout << ", ";
            }
            std::cout << "w" << gate.inputs[i]
                      << "(" << circuit.Wire(gate.inputs[i]).name << ")";
        }
        std::cout << ")" << std::endl;
    }

#ifdef MOCK_OPENFHE
#ifdef USE_EMP_GC
    std::cout << "EMP GC evaluated [a <= b] = "
              << (predicate_gc.Evaluate(enc_a, enc_b) ? 1 : 0) << std::endl;
#else
    std::cout << "Minimal GC evaluated [a <= b] = "
              << (predicate_gc.Evaluate(enc_a, enc_b) ? 1 : 0) << std::endl;
#endif
#else
    std::cout << "GC direct evaluation is shown only in MOCK_OPENFHE mode."
              << std::endl;
#endif
    
    // 4. Evaluator loop: runtime and predicate bits are intentionally revealed.
    auto state = enc_a;
    size_t iterations = 0;
    while (true) {
        bool predicate = predicate_gc.Evaluate(state, enc_b);
        std::cout << "GC_f(x', b') revealed predicate [x <= b] = "
                  << (predicate ? 1 : 0) << std::endl;

        if (!predicate) {
            break;
        }

        ++iterations;
        state = arithmetic.Increment(state);
    }

    std::cout << "Encrypted loop iterations executed: " << iterations << std::endl;
    std::cout << "Demo completed without decrypting a, b, or x." << std::endl;
    return 0;
}

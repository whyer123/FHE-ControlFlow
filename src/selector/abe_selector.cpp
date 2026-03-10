#include "src/selector/selector_api.h"
#include <iostream>

class ABESelector : public SelectorAPI {
public:
  ABESelector(FHEContextWrapper &ctx) : fhe_ctx(ctx) {
    std::cout << "[ABESelector] Initialized (Single-Party Simulation Mode)"
              << std::endl;
    std::cout << "[ABESelector] Simulating offline evaluation using an "
                 "ABE-encrypted Garbled Circuit."
              << std::endl;
  }

  // In GKP13, the Client provides ABE decryption keys (tokens) corresponding to
  // the states. The Evaluator (Cloud) evaluates the condition without any
  // network interaction.
  bool ExtractConditionBit(const LWECiphertext &cond_bit) override {
    // --- SIMULATION START ---
    // In a true ABE scheme, this evaluation would use the ABE token to blindly
    // transition the state only if cond_bit encrypted to 1.
    // Since we are simulating the Control Flow mechanism (and C++ ABE libraries
    // don't currently support LWE ciphertext integration), we simulate the
    // "offline evaluation" by decrypting it locally in this simulation
    // selector.

    LWEPlaintext debug_pt;
    fhe_ctx.GetContext().Decrypt(fhe_ctx.GetSecretKey(), cond_bit, &debug_pt);
    return debug_pt == 1;
    // --- SIMULATION END ---
  }

private:
  FHEContextWrapper &fhe_ctx;
};

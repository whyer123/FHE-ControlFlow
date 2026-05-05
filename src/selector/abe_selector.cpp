#include "src/selector/selector_api.h"
#include <iostream>

ABESelector::ABESelector(FHEContextWrapper& ctx)
    : ProgrammedSelectorBase(0x4142453250524F54ULL), fhe_ctx(ctx) {
  std::cout << "[ABESelector] Initialized (ABE2 offline transition prototype)"
            << std::endl;
  std::cout << "[ABESelector] Evaluator will advance using state tokens without "
               "client interaction."
            << std::endl;
}

bool ABESelector::ExtractBranch(const LWECiphertext& cond_bit) {
  // This is still a prototype: the branch is recovered locally so we can model
  // ABE2 state-token transitions with the current FHE-only codebase.
  LWEPlaintext plain_bit;
  fhe_ctx.GetContext().Decrypt(fhe_ctx.GetSecretKey(), cond_bit, &plain_bit);
  return plain_bit == 1;
}

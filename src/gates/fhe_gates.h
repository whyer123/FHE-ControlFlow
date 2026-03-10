#pragma once

#include "src/fhe/fhe_context.h"

class FHEGates {
public:
  FHEGates(FHEContextWrapper &fhe) : fhe_ctx(fhe) {}

  LWECiphertext EvalAND(const LWECiphertext &ct1, const LWECiphertext &ct2);
  LWECiphertext EvalXNOR(const LWECiphertext &ct1, const LWECiphertext &ct2);
  LWECiphertext EvalXOR(const LWECiphertext &ct1, const LWECiphertext &ct2);
  LWECiphertext EvalOR(const LWECiphertext &ct1, const LWECiphertext &ct2);
  LWECiphertext EvalNOT(const LWECiphertext &ct);

  // MUX: if (sel == 1) return ct1 else return ct2
  LWECiphertext EvalMUX(const LWECiphertext &sel, const LWECiphertext &ct1,
                        const LWECiphertext &ct2);

private:
  FHEContextWrapper &fhe_ctx;
};

#pragma once

#ifdef MOCK_OPENFHE
#include "src/fhe/mock_openfhe.h"
#else
#include "binfhecontext.h"
#endif

using namespace lbcrypto;

class FHEContextWrapper {
public:
  FHEContextWrapper();
  ~FHEContextWrapper() = default;

  // Encrypts an integer bit-by-bit into an array of ciphertexts
  std::vector<LWECiphertext> EncryptInteger(int64_t value,
                                            size_t bit_length = 32);

  // Decrypts an array of ciphertexts into an integer
  int64_t DecryptInteger(const std::vector<LWECiphertext> &cipher_bits);

  BinFHEContext &GetContext() { return cc; }
  LWEPrivateKey GetSecretKey() const { return sk; }

private:
  BinFHEContext cc;
  LWEPrivateKey sk;
};

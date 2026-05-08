# OpenFHE Demo Encrypted Constants

This directory includes fixed encrypted constants for the offline loop demo.

## one_prime

- Plaintext: `1`
- Encryption key: `hpk_lwe_public_key.*`
- Ciphertext JSON: `one_prime_lwe_ciphertext.json`
- Ciphertext binary: `one_prime_lwe_ciphertext.bin`
- Verification: `Dec_hsk(one_prime) = 1`

Evaluator loop usage:

```text
x' <- FHE.Add(x', one')
```

The evaluator should receive `one'` as a prepared ciphertext and does not need `hpk` to encrypt the constant itself.

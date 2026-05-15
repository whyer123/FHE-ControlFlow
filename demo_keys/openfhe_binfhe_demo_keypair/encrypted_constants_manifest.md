# OpenFHE Demo Encrypted Constants

This directory includes fixed encrypted endpoints and constants for the offline loop demo.

## Fixed integer material

- Demo bit length: `4`
- `a = 3`: `a_prime_bit_0..3_lwe_ciphertext.*`
- `b = 7`: `b_prime_bit_0..3_lwe_ciphertext.*`
- Integer `one' = Enc(1)`: `one_prime_integer_bit_0..3_lwe_ciphertext.*`
- Verification: every bit was checked with `hsk` during setup.

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

The evaluator should receive integer `one'` as prepared ciphertext bits and does not need `hpk` to encrypt the constant itself. In the fixed-bound GC runtime, `b'` is setup material for `GC_f`; it is not a free runtime input.

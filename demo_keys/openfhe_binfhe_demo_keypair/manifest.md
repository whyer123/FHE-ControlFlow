# OpenFHE BinFHE Demo Key Pair

This directory contains one generated OpenFHE BinFHE/LWE key pair for the fixed demo direction.

## Files

- `hpk_lwe_public_key.json`: JSON-serialized LWE public key.
- `hpk_lwe_public_key.bin`: binary-serialized LWE public key.
- `hsk_lwe_secret_key.json`: JSON-serialized LWE secret key.
- `hsk_lwe_secret_key.bin`: binary-serialized LWE secret key.
- `binfhe_context_params.bin`: BinFHE context/parameter material.
- `eval_refresh_key.bin`: refresh/bootstrapping evaluation key.
- `eval_switch_key.bin`: switching evaluation key.

## Parameters

- OpenFHE context: `BinFHEContext`
- Parameter set: `TOY`
- Public-key encryption self-test: `Enc_hpk(0/1)` then `Dec_hsk`
- Gate self-test: `EvalBinGate(AND, Enc_hpk(1), Enc_hpk(1)) = 1`

## Size note

The LWE public key is large because it contains many LWE public-key samples. The JSON file expands vectors into decimal text and is much larger than the binary form; runtime code should prefer `.bin` files. The switching evaluation key is also large and is separate from `hpk`.

These files are demo material only and are not production security parameters.

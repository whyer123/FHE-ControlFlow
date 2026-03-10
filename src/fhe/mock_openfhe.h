#pragma once

#include <iostream>

// Mock OpenFHE classes for fast local prototyping without the real library.
// It uses standard boolean logic instead of actual FHE cryptography.

namespace lbcrypto {

    class LWECiphertext {
    public:
        bool bit;
        LWECiphertext() : bit(false) {}
        LWECiphertext(bool b) : bit(b) {}
    };

    enum BINGATE { AND, OR, XOR, XNOR };

    class LWEPlaintext {
    public:
        int value;
        bool operator==(int v) const { return value == v; }
        LWEPlaintext& operator=(int v) { value = v; return *this; }
    };

    class LWEPrivateKey {
    public:
        int dummy;
    };

    enum BINFHE_PARAMSET { TOY, STD128 };

    class BinFHEContext {
    public:
        void GenerateBinFHEContext(BINFHE_PARAMSET) {
            // No-op
        }
        LWEPrivateKey KeyGen() {
            return LWEPrivateKey(); 
        }
        void BTKeyGen(LWEPrivateKey) {
            // No-op
        }

        LWECiphertext Encrypt(LWEPrivateKey, int bit) { 
            return LWECiphertext(bit != 0); 
        }
        
        void Decrypt(LWEPrivateKey, LWECiphertext ct, LWEPlaintext* pt) {
            pt->value = ct.bit ? 1 : 0;
        }

        LWECiphertext EvalBinGate(BINGATE gate, LWECiphertext ct1, LWECiphertext ct2) {
            switch(gate) {
                case AND: return ct1.bit & ct2.bit;
                case OR: return ct1.bit | ct2.bit;
                case XOR: return ct1.bit ^ ct2.bit;
                case XNOR: return !(ct1.bit ^ ct2.bit);
                default: return false;
            }
        }

        LWECiphertext EvalNOT(LWECiphertext ct) {
            return !ct.bit;
        }
    };
}

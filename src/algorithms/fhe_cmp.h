#pragma once

#include "src/gates/fhe_gates.h"
#include <vector>

class FHECompare {
public:
  FHECompare(FHEGates &gates) : fhe_gates(gates) {}

  // Check if a n-bit ciphertext integer is equal to 0
  LWECiphertext EqualToZero(const std::vector<LWECiphertext> &x);

  // Check if two n-bit ciphertext integers are equal
  LWECiphertext Equal(const std::vector<LWECiphertext> &x,
                      const std::vector<LWECiphertext> &y);

  // Returns a condition bit (1 if x > y else 0)
  LWECiphertext GreaterThan(const std::vector<LWECiphertext> &x,
                            const std::vector<LWECiphertext> &y);

  // Returns a condition bit (1 if x < y else 0)
  LWECiphertext LessThan(const std::vector<LWECiphertext> &x,
                         const std::vector<LWECiphertext> &y);

  // Returns the max of x and y
  std::vector<LWECiphertext> FindMax(const std::vector<LWECiphertext> &x,
                                     const std::vector<LWECiphertext> &y);

  // Returns the min of x and y
  std::vector<LWECiphertext> FindMin(const std::vector<LWECiphertext> &x,
                                     const std::vector<LWECiphertext> &y);

private:
  FHEGates &fhe_gates;
};

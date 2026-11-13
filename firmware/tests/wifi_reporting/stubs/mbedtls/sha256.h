#pragma once
#include <Arduino.h>

// Device fingerprinting is outside the recovery scenario; provide deterministic
// bytes at the crypto boundary so the production URL builder still runs.
inline int mbedtls_sha256_ret(const uint8_t*, size_t, uint8_t* digest, int) {
  for (size_t index = 0; index < 32; ++index) digest[index] = static_cast<uint8_t>(index);
  return 0;
}

/* Copyright (c) 2024 vesoft inc. All rights reserved.
 *
 * This source code is licensed under Apache 2.0 License.
 */

#ifndef META_PASSWORDUTILS_H_
#define META_PASSWORDUTILS_H_

#include <openssl/rand.h>
#include <openssl/sha.h>

#include <cstdio>
#include <cstddef>
#include <string>

namespace nebula {
namespace meta {

// Stored password format: $sha256$<32-hex-salt>$<64-hex-hash>
// Legacy format (no prefix): raw MD5 hex — accepted for backward compatibility.
class PasswordUtils {
 public:
  // Wrap `input` (typically the client-sent MD5 hex) in SHA-256 + random salt.
  static std::string hashPassword(const std::string& input) {
    unsigned char salt[16];
    RAND_bytes(salt, sizeof(salt));
    std::string saltHex = toHex(salt, sizeof(salt));
    std::string hash = sha256Hex(saltHex + input);
    return "$sha256$" + saltHex + "$" + hash;
  }

  // Returns true when `input` matches `stored`.
  // Handles both new ($sha256$…) and legacy (raw MD5) stored values.
  static bool verifyPassword(const std::string& input, const std::string& stored) {
    if (stored.size() > 8 && stored.substr(0, 8) == "$sha256$") {
      // New format: $sha256$<32-hex-salt>$<64-hex-hash>
      auto dollarPos = stored.find('$', 8);
      if (dollarPos == std::string::npos) {
        return false;
      }
      std::string saltHex = stored.substr(8, dollarPos - 8);
      std::string expectedHash = stored.substr(dollarPos + 1);
      return sha256Hex(saltHex + input) == expectedHash;
    }
    // Legacy: raw string equality (old MD5 path — migration only).
    return stored == input;
  }

 private:
  static std::string sha256Hex(const std::string& input) {
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256(reinterpret_cast<const unsigned char*>(input.data()), input.size(), hash);
    return toHex(hash, SHA256_DIGEST_LENGTH);
  }

  static std::string toHex(const unsigned char* data, size_t len) {
    std::string result;
    result.reserve(len * 2);
    for (size_t i = 0; i < len; ++i) {
      char buf[3];
      std::snprintf(buf, sizeof(buf), "%02x", data[i]);
      result.append(buf, 2);
    }
    return result;
  }
};

}  // namespace meta
}  // namespace nebula

#endif  // META_PASSWORDUTILS_H_

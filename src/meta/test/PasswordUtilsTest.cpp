/* Copyright (c) 2024 vesoft inc. All rights reserved.
 *
 * This source code is licensed under Apache 2.0 License.
 */

#include <gtest/gtest.h>

#include "meta/PasswordUtils.h"

namespace nebula {
namespace meta {

TEST(PasswordUtilsTest, StoredFormatHasSha256Prefix) {
  auto stored = PasswordUtils::hashPassword("mypassword");
  ASSERT_EQ(stored.substr(0, 8), "$sha256$") << "stored value must start with $sha256$";
}

TEST(PasswordUtilsTest, StoredValueIsNotPlaintext) {
  std::string password = "nebula";
  auto stored = PasswordUtils::hashPassword(password);
  ASSERT_NE(stored, password) << "stored value must not equal plaintext";
}

TEST(PasswordUtilsTest, RandomSaltProducesDifferentHashEachTime) {
  auto stored1 = PasswordUtils::hashPassword("samepassword");
  auto stored2 = PasswordUtils::hashPassword("samepassword");
  ASSERT_NE(stored1, stored2) << "two hashes of same input must differ due to random salt";
}

TEST(PasswordUtilsTest, VerifyCorrectPassword) {
  auto stored = PasswordUtils::hashPassword("correct");
  ASSERT_TRUE(PasswordUtils::verifyPassword("correct", stored));
}

TEST(PasswordUtilsTest, VerifyWrongPasswordFails) {
  auto stored = PasswordUtils::hashPassword("correct");
  ASSERT_FALSE(PasswordUtils::verifyPassword("wrong", stored));
}

TEST(PasswordUtilsTest, VerifyEmptyPasswordFails) {
  auto stored = PasswordUtils::hashPassword("nonempty");
  ASSERT_FALSE(PasswordUtils::verifyPassword("", stored));
}

TEST(PasswordUtilsTest, VerifyHashedEmptyPassword) {
  auto stored = PasswordUtils::hashPassword("");
  ASSERT_TRUE(PasswordUtils::verifyPassword("", stored));
  ASSERT_FALSE(PasswordUtils::verifyPassword("nonempty", stored));
}

TEST(PasswordUtilsTest, LegacyPlaintextFallback) {
  // Passwords stored before this fix have no $sha256$ prefix.
  // verifyPassword must still match them via direct string comparison
  // to preserve backward compatibility during migration.
  std::string legacyStored = "d8578edf8458ce06fbc5bb76a58c5ca4";  // legacy MD5, no prefix
  ASSERT_TRUE(PasswordUtils::verifyPassword("d8578edf8458ce06fbc5bb76a58c5ca4", legacyStored));
  ASSERT_FALSE(PasswordUtils::verifyPassword("differentvalue", legacyStored));
}

TEST(PasswordUtilsTest, MalformedSha256StoredReturnsFalse) {
  // $sha256$ prefix but missing second $ separator — must not crash.
  ASSERT_FALSE(PasswordUtils::verifyPassword("anything", "$sha256$noseparatorhere"));
}

TEST(PasswordUtilsTest, StoredFormatHasTwoDelimiters) {
  auto stored = PasswordUtils::hashPassword("test");
  // Format: $sha256$<32-hex-salt>$<64-hex-hash>
  // Expect exactly 3 '$'-separated parts.
  size_t first = stored.find('$', 1);
  size_t second = stored.find('$', first + 1);
  ASSERT_NE(first, std::string::npos);
  ASSERT_NE(second, std::string::npos);
  std::string saltHex = stored.substr(first + 1, second - first - 1);
  std::string hashHex = stored.substr(second + 1);
  ASSERT_EQ(saltHex.size(), 32u)  << "salt must be 32 hex chars (16 bytes)";
  ASSERT_EQ(hashHex.size(), 64u)  << "hash must be 64 hex chars (32 bytes / SHA-256)";
}

}  // namespace meta
}  // namespace nebula

int main(int argc, char** argv) {
  testing::InitGoogleTest(&argc, argv);
  folly::init(&argc, &argv, true);
  google::SetStderrLogging(google::INFO);
  return RUN_ALL_TESTS();
}

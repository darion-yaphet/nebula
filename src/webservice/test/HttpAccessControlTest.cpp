/* Copyright (c) 2026 vesoft inc. All rights reserved.
 *
 * This source code is licensed under Apache 2.0 License.
 */

// This policy test also runs without the full database/Thrift build. It uses
// explicit checks rather than assert(), so Release builds retain every check.
#include <iostream>
#include <string>
#include <vector>

#include "webservice/HttpAccessControl.h"

int main() {
  int checks = 0;
  int failures = 0;
  auto check = [&](bool condition, const char* message) {
    ++checks;
    if (!condition) {
      ++failures;
      std::cerr << message << '\n';
    }
  };
  const std::string token(64, 'a');
  const std::string credential = "Bearer " + token;
  nebula::HttpAccessControl disabled("");
  check(!disabled.enabled(), "Authentication must be optional when no token is configured");
  check(disabled.authorized(""), "Legacy requests without credentials must remain accessible");
  check(disabled.authorized(credential), "Legacy mode must ignore supplied credentials");
  nebula::HttpAccessControl weak("short");
  check(!weak.authorized("Bearer short"), "Invalid configured tokens must not grant access");

  nebula::HttpAccessControl access(token);
  check(access.enabled(), "A configured token must enable authentication");
  check(access.authorized(credential), "The exact credential must authenticate");
  for (const auto& bad : std::vector<std::string>{"",
                                                  token,
                                                  "Basic " + token,
                                                  "Bearer ",
                                                  credential + "a",
                                                  credential.substr(0, 63),
                                                  "Bearer " + std::string(64, 'b'),
                                                  credential + ", " + credential,
                                                  credential + "\r\n",
                                                  credential + std::string(1, '\0')}) {
    check(!access.authorized(bad), "Malformed or wrong credentials must be denied");
  }
  auto changed = credential;
  changed[7] = 'b';
  check(!access.authorized(changed), "A mismatched first token byte must be denied");
  changed = credential;
  changed.back() = 'b';
  check(!access.authorized(changed), "A mismatched final token byte must be denied");

  for (const auto* name : {"v", "minloglevel", "log_dir", "slow_query_threshold_us"}) {
    check(access.canSetFlag(name), "Authenticated callers must not need a per-flag allowlist");
  }
  for (const auto& flag : {"ws_http_token_file",
                           "ws_ip",
                           "ws_http_port",
                           "enable_authorize",
                           "auth_type",
                           "enable_ssl",
                           "enable_graph_ssl",
                           "enable_meta_ssl",
                           "cert_path",
                           "key_path",
                           "password_path",
                           "ca_path",
                           "flagfile",
                           "fromenv",
                           "tryfromenv",
                           "cloud_http_url",
                           "enable_udf",
                           "udf_path",
                           "failed_login_attempts",
                           "password_lock_time_in_secs",
                           "v\n",
                           "--v",
                           "v/../ws_ip"}) {
    check(!access.canSetFlag(flag), "Security flags and invalid names must remain protected");
  }
  auto nulName = "v" + std::string(1, '\0') + "ws_http_token_file";
  check(!access.canSetFlag(nulName), "NUL-terminated flag-name aliases must be denied");

  check(nebula::HttpAccessControl::validToken(std::string(32, 'a')),
        "32-byte tokens must be valid");
  check(nebula::HttpAccessControl::validToken(std::string(256, 'a')),
        "256-byte tokens must be valid");
  for (const auto& bad : std::vector<std::string>{"",
                                                  std::string(31, 'a'),
                                                  std::string(257, 'a'),
                                                  token + " ",
                                                  token + "\n",
                                                  token + ",",
                                                  token + std::string(1, '\0')}) {
    check(!nebula::HttpAccessControl::validToken(bad), "Invalid token files must be rejected");
  }
  std::cout << checks << " policy checks, " << failures << " failures\n";
  return failures == 0 ? 0 : 1;
}

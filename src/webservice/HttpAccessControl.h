/* Copyright (c) 2026 vesoft inc. All rights reserved.
 *
 * This source code is licensed under Apache 2.0 License.
 */

#ifndef WEBSERVICE_HTTPACCESSCONTROL_H_
#define WEBSERVICE_HTTPACCESSCONTROL_H_

#include <openssl/crypto.h>

#include <algorithm>
#include <string>
#include <unordered_set>

namespace nebula {

// A startup snapshot: neither /flags nor later file changes can relax this policy.
class HttpAccessControl final {
 public:
  explicit HttpAccessControl(const std::string& token)
      : enabled_(!token.empty()), credential_(validToken(token) ? "Bearer " + token : "") {}

  static bool validToken(const std::string& token) {
    return token.size() >= 32 && token.size() <= 256 &&
           std::all_of(token.begin(), token.end(), [](unsigned char c) {
             return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
                    c == '-' || c == '.' || c == '_' || c == '~' || c == '+' || c == '/' ||
                    c == '=';
           });
  }

  bool enabled() const {
    return enabled_;
  }

  bool authorized(const std::string& authorization) const {
    // An unset token preserves legacy anonymous access. A nonempty but invalid
    // token must never silently disable authentication.
    return !enabled() ||
           (!credential_.empty() && authorization.size() == credential_.size() &&
            CRYPTO_memcmp(authorization.data(), credential_.data(), credential_.size()) == 0);
  }

  bool canSetFlag(const std::string& name) const {
    if (name.empty() || name.compare(0, 3, "ws_") == 0 ||
        !std::all_of(name.begin(), name.end(), [](unsigned char c) {
          return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
                 c == '_';
        })) {
      return false;
    }
    static const std::unordered_set<std::string> securityFlags = {"enable_authorize",
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
                                                                  "enable_client_white_list",
                                                                  "client_white_list"};
    return securityFlags.count(name) == 0;
  }

 private:
  const bool enabled_;
  const std::string credential_;
};

}  // namespace nebula

#endif  // WEBSERVICE_HTTPACCESSCONTROL_H_

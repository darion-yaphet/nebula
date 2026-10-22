/* Copyright (c) 2019 vesoft inc. All rights reserved.
 *
 * This source code is licensed under Apache 2.0 License.
 */

#ifndef WEBSERVICE_TEST_TESTUTILS_H_
#define WEBSERVICE_TEST_TESTUTILS_H_

#include <fstream>

#include "common/base/Base.h"
#include "common/fs/TempFile.h"
#include "common/http/HttpClient.h"
#include "webservice/WebService.h"

namespace nebula {

constexpr char kHttpAuthTestToken[] = "nebula-http-admin-test-token-0123456789";

class HttpAuthTestGuard final {
 public:
  HttpAuthTestGuard() : tokenFile_("/tmp/nebula-http-auth-test.XXXXXX") {
    std::ofstream out(tokenFile_.path(), std::ios::out | std::ios::trunc);
    CHECK(out.good()) << "Could not open HTTP auth token test file";
    out << kHttpAuthTestToken << '\n';
    out.close();
    CHECK(out.good()) << "Could not write HTTP auth token test file";

    FLAGS_ws_http_token_file = tokenFile_.path();
  }

  HttpAuthTestGuard(const HttpAuthTestGuard&) = delete;
  HttpAuthTestGuard& operator=(const HttpAuthTestGuard&) = delete;

 private:
  gflags::FlagSaver flagSaver_;
  fs::TempFile tokenFile_;
};

inline std::string webServiceUrl(const std::string& urlPath) {
  return folly::stringPrintf(
      "http://%s:%d%s", FLAGS_ws_ip.c_str(), FLAGS_ws_http_port, urlPath.c_str());
}

inline std::vector<std::string> httpAuthHeaders() {
  return {folly::stringPrintf("Authorization: Bearer %s", kHttpAuthTestToken)};
}

inline bool hasHttpStatus(const HttpResponse& response, int status) {
  return response.header.find(folly::stringPrintf(" %d ", status)) != std::string::npos;
}

inline bool getUrl(const std::string& urlPath, std::string& respBody) {
  auto url = webServiceUrl(urlPath);
  VLOG(1) << "Retrieving url: " << url;

  auto httpResp = HttpClient::instance().get(url.c_str(), httpAuthHeaders());
  if (httpResp.curlCode != 0) {
    std::string error = fmt::format("curl error({}): {}", httpResp.curlCode, httpResp.curlMessage);
    LOG(ERROR) << error;
    return false;
  }

  respBody = httpResp.body;
  return true;
}

inline StatusOr<std::string> putUrl(const std::string& urlPath, const folly::dynamic& data) {
  auto url = webServiceUrl(urlPath);
  VLOG(1) << "Retrieving url: " << url;

  auto httpResp = HttpClient::instance().put(url.c_str(), httpAuthHeaders(), folly::toJson(data));
  if (httpResp.curlCode != 0) {
    std::string error = fmt::format("curl error({}): {}", httpResp.curlCode, httpResp.curlMessage);
    LOG(ERROR) << error;
    return Status::Error(error);
  }
  return httpResp.body;
}

}  // namespace nebula

#endif  // WEBSERVICE_TEST_TESTUTILS_H_

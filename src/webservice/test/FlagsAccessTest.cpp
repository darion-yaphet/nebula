/* Copyright (c) 2018 vesoft inc. All rights reserved.
 *
 * This source code is licensed under Apache 2.0 License.
 */

#include <folly/json.h>
#include <gtest/gtest.h>
#include <sys/stat.h>
#include <unistd.h>

#include <atomic>
#include <fstream>

#include "common/base/Base.h"
#include "webservice/Router.h"
#include "webservice/StatusHandler.h"
#include "webservice/WebService.h"
#include "webservice/test/TestUtils.h"

DEFINE_int32(int32_test, 10, "Test flag for int32 type");
DEFINE_int64(int64_test, 10, "Test flag for int64 type");
DEFINE_bool(bool_test, false, "Test flag for bool type");
DEFINE_double(double_test, 3.14159, "Test flag for double type");
DEFINE_string(string_test, "Hello World", "Test flag for string type");
DEFINE_uint32(uint32_test, 10, "Test flag for uint32 type");
DEFINE_uint64(uint64_test, 10, "Test flag for uint64 type");

namespace nebula {

namespace {

HttpResponse getRaw(const std::string& path, const std::vector<std::string>& headers = {}) {
  return HttpClient::instance().get(webServiceUrl(path), headers);
}

HttpResponse putRaw(const std::string& body,
                    const std::vector<std::string>& headers = httpAuthHeaders()) {
  return HttpClient::instance().put(webServiceUrl("/flags"), headers, body);
}

}  // namespace

class FlagsAccessTestEnv : public ::testing::Environment {
 public:
  void SetUp() override {
    FLAGS_ws_http_port = 0;
    authGuard_ = std::make_unique<HttpAuthTestGuard>();
    VLOG(1) << "Starting web service...";
    webSvc_ = std::make_unique<WebService>();
    auto status = webSvc_->start();
    ASSERT_TRUE(status.ok()) << status;
  }

  void TearDown() override {
    webSvc_.reset();
    authGuard_.reset();
    VLOG(1) << "Web service stopped";
  }

 private:
  std::unique_ptr<WebService> webSvc_;
  std::unique_ptr<HttpAuthTestGuard> authGuard_;
};

TEST(FlagsAccessTest, AuthenticationRequired) {
  gflags::FlagSaver saver;
  const auto original = FLAGS_int32_test;

  for (const auto* path : {"/flags", "/flags/?flags=int32_test", "/admin", "/replace"}) {
    auto response = getRaw(path);
    ASSERT_EQ(CURLE_OK, response.curlCode);
    EXPECT_TRUE(hasHttpStatus(response, 401)) << "path=" << path << " " << response.header;
  }

  auto missing = putRaw(R"({"int32_test":121})", {});
  ASSERT_EQ(CURLE_OK, missing.curlCode);
  EXPECT_TRUE(hasHttpStatus(missing, 401)) << missing.header;
  EXPECT_NE(std::string::npos, missing.header.find("WWW-Authenticate: Bearer"));
  EXPECT_EQ(original, FLAGS_int32_test);

  auto wrong = putRaw(R"({"int32_test":123})", {"Authorization: Bearer wrong-token"});
  ASSERT_EQ(CURLE_OK, wrong.curlCode);
  EXPECT_TRUE(hasHttpStatus(wrong, 401)) << wrong.header;
  EXPECT_EQ(original, FLAGS_int32_test);

  auto duplicate = putRaw(R"({"int32_test":122})",
                          {folly::stringPrintf("Authorization: Bearer %s", kHttpAuthTestToken),
                           folly::stringPrintf("Authorization: Bearer %s", kHttpAuthTestToken)});
  ASSERT_EQ(CURLE_OK, duplicate.curlCode);
  EXPECT_TRUE(hasHttpStatus(duplicate, 401)) << duplicate.header;
  EXPECT_EQ(original, FLAGS_int32_test);

  auto authorized = getRaw("/flags?flags=int32_test", httpAuthHeaders());
  ASSERT_EQ(CURLE_OK, authorized.curlCode);
  EXPECT_TRUE(hasHttpStatus(authorized, 200)) << authorized.header;

  // Authentication settings are startup snapshots, not live gflags.
  FLAGS_ws_http_token_file.clear();
  auto afterFlagChange = getRaw("/flags?flags=int32_test", httpAuthHeaders());
  ASSERT_EQ(CURLE_OK, afterFlagChange.curlCode);
  EXPECT_TRUE(hasHttpStatus(afterFlagChange, 200)) << afterFlagChange.header;
  auto stillProtected = getRaw("/flags?flags=int32_test");
  ASSERT_EQ(CURLE_OK, stillProtected.curlCode);
  EXPECT_TRUE(hasHttpStatus(stillProtected, 401)) << stillProtected.header;
}

TEST(FlagsAccessTest, MatchedRouteDeterminesPublicAccess) {
  gflags::FlagSaver saver;
  FLAGS_ws_http_port = 0;
  HttpAuthTestGuard authGuard;
  std::atomic<int> dispatchCount{0};

  WebService service;
  service.router().get("/:operation").handler([&](web::PathParams&&) {
    dispatchCount.fetch_add(1);
    return new StatusHandler();
  });
  ASSERT_TRUE(service.start(0).ok());

  auto missing = HttpClient::instance().get(webServiceUrl("/stats"));
  ASSERT_EQ(CURLE_OK, missing.curlCode);
  EXPECT_TRUE(hasHttpStatus(missing, 401)) << missing.header;
  EXPECT_EQ(0, dispatchCount.load());

  auto authorized = HttpClient::instance().get(webServiceUrl("/stats"), httpAuthHeaders());
  ASSERT_EQ(CURLE_OK, authorized.curlCode);
  EXPECT_TRUE(hasHttpStatus(authorized, 200)) << authorized.header;
  EXPECT_EQ(1, dispatchCount.load());
}

TEST(FlagsAccessTest, PublicReadOnlyEndpointsDoNotRequireToken) {
  auto status = getRaw("/status");
  ASSERT_EQ(CURLE_OK, status.curlCode);
  EXPECT_TRUE(hasHttpStatus(status, 200)) << status.header;

  auto stats = getRaw("/stats");
  ASSERT_EQ(CURLE_OK, stats.curlCode);
  EXPECT_TRUE(hasHttpStatus(stats, 200)) << stats.header;
}

TEST(FlagsAccessTest, NoTokenConfigurationPreservesAnonymousAccess) {
  gflags::FlagSaver saver;
  FLAGS_ws_http_port = 0;
  FLAGS_ws_http_token_file.clear();
  std::atomic<int> dispatchCount{0};

  WebService service;
  for (const auto* path : {"/admin", "/replace"}) {
    service.router().get(path).handler([&](web::PathParams&&) {
      dispatchCount.fetch_add(1);
      return new StatusHandler();
    });
  }
  ASSERT_TRUE(service.start(0).ok());
  for (const auto* path : {"/flags?flags=int32_test", "/admin", "/replace", "/status", "/stats"}) {
    auto response = HttpClient::instance().get(webServiceUrl(path));
    ASSERT_EQ(CURLE_OK, response.curlCode);
    EXPECT_TRUE(hasHttpStatus(response, 200)) << "path=" << path << " " << response.header;
  }
  EXPECT_EQ(2, dispatchCount.load());
  auto update = HttpClient::instance().put(webServiceUrl("/flags"), {}, R"({"int32_test":123})");
  ASSERT_EQ(CURLE_OK, update.curlCode);
  EXPECT_TRUE(hasHttpStatus(update, 200)) << update.header;
  EXPECT_EQ(123, FLAGS_int32_test);

  // Configuring a token later must not change the startup policy.
  FLAGS_ws_http_token_file = "/not-loaded-until-restart";
  auto anonymous = HttpClient::instance().get(webServiceUrl("/flags?flags=int32_test"));
  ASSERT_EQ(CURLE_OK, anonymous.curlCode);
  EXPECT_TRUE(hasHttpStatus(anonymous, 200)) << anonymous.header;
}

TEST(FlagsAccessTest, InvalidTokenFilesPreventStartup) {
  gflags::FlagSaver saver;
  FLAGS_ws_http_port = 0;
  fs::TempFile tokenFile("/tmp/nebula-invalid-http-token.XXXXXX");
  {
    std::ofstream out(tokenFile.path());
    out << "too-short\n";
  }
  FLAGS_ws_http_token_file = tokenFile.path();

  WebService service;
  EXPECT_FALSE(service.start(0).ok());
}

TEST(FlagsAccessTest, EmptyAndMultilineTokenFilesPreventStartup) {
  gflags::FlagSaver saver;
  for (const auto& contents :
       {std::string(), std::string(kHttpAuthTestToken) + "\nsecond-valid-token-line\n"}) {
    FLAGS_ws_http_port = 0;
    fs::TempFile tokenFile("/tmp/nebula-malformed-http-token.XXXXXX");
    {
      std::ofstream out(tokenFile.path());
      out << contents;
    }
    FLAGS_ws_http_token_file = tokenFile.path();

    WebService service;
    EXPECT_FALSE(service.start(0).ok());
  }

  FLAGS_ws_http_port = 0;
  std::string missingPath;
  {
    fs::TempFile removedFile("/tmp/nebula-missing-http-token.XXXXXX");
    missingPath = removedFile.path();
  }
  FLAGS_ws_http_token_file = missingPath;
  WebService missingFileService;
  EXPECT_FALSE(missingFileService.start(0).ok());
}

TEST(FlagsAccessTest, InsecureTokenFilePermissionsPreventStartup) {
  gflags::FlagSaver saver;
  FLAGS_ws_http_port = 0;
  fs::TempFile tokenFile("/tmp/nebula-insecure-http-token.XXXXXX");
  {
    std::ofstream out(tokenFile.path());
    out << kHttpAuthTestToken << '\n';
  }
  ASSERT_EQ(0, ::chmod(tokenFile.path(), 0644));
  FLAGS_ws_http_token_file = tokenFile.path();

  WebService service;
  EXPECT_FALSE(service.start(0).ok());
}

TEST(FlagsAccessTest, DifferentTokenFileOwnerPreventsStartup) {
  if (::geteuid() != 0) {
    GTEST_SKIP() << "Changing a token file to a different owner requires root";
  }
  gflags::FlagSaver saver;
  FLAGS_ws_http_port = 0;
  fs::TempFile tokenFile("/tmp/nebula-other-owner-http-token.XXXXXX");
  {
    std::ofstream out(tokenFile.path());
    out << kHttpAuthTestToken << '\n';
  }
  ASSERT_EQ(0, ::chown(tokenFile.path(), 1, static_cast<gid_t>(-1)));
  FLAGS_ws_http_token_file = tokenFile.path();

  WebService service;
  EXPECT_FALSE(service.start(0).ok());
}

TEST(FlagsAccessTest, GetSetTest) {
  std::string resp;
  ASSERT_TRUE(getUrl("/flags?flags=int32_test", resp));
  EXPECT_EQ(folly::stringPrintf("int32_test=%d\n", FLAGS_int32_test), resp);

  ASSERT_TRUE(getUrl("/flags?flags=int64_test,bool_test,string_test", resp));
  EXPECT_EQ(folly::stringPrintf("int64_test=%ld\nbool_test=%s\nstring_test=\"%s\"\n",
                                FLAGS_int64_test,
                                (FLAGS_bool_test ? "1" : "0"),
                                FLAGS_string_test.c_str()),
            resp);

  folly::dynamic data = folly::dynamic::object("int64_test", 20);
  auto status = putUrl("/flags", data);
  ASSERT_TRUE(status.ok());
  folly::dynamic json = folly::parseJson(status.value());
  ASSERT_EQ(0, json["errCode"].asInt());
  ASSERT_TRUE(getUrl("/flags?flags=int64_test", resp));
  EXPECT_EQ(std::string("int64_test=20\n"), resp);

  ASSERT_TRUE(getUrl("/flags?flags=uint32_test", resp));
  EXPECT_EQ(folly::stringPrintf("uint32_test=%u\n", FLAGS_uint32_test), resp);

  ASSERT_TRUE(getUrl("/flags?flags=uint64_test", resp));
  EXPECT_EQ(folly::stringPrintf("uint64_test=%lu\n", FLAGS_uint64_test), resp);
}

TEST(FlagsAccessTest, TestSetFlagsFailure) {
  folly::dynamic data = folly::dynamic::object("int64_test", 20)("float_test", 10.0f);
  auto status = putUrl("/flags", data);
  ASSERT_TRUE(status.ok());
  folly::dynamic json = folly::parseJson(status.value());
  folly::dynamic failedOptions = json["failedOptions"];
  ASSERT_TRUE(failedOptions.isArray());
  ASSERT_EQ(failedOptions, folly::dynamic::array("float_test"));
}

TEST(FlagsAccessTest, SetFlagsRejectsSecurityChangesBeforeWriting) {
  gflags::FlagSaver saver;
  const auto original = FLAGS_int32_test;

  auto denied = putRaw(R"({"int32_test":123,"enable_authorize":true})");
  ASSERT_EQ(CURLE_OK, denied.curlCode);
  EXPECT_TRUE(hasHttpStatus(denied, 403)) << denied.header;
  EXPECT_EQ(original, FLAGS_int32_test);

  auto webFlag = putRaw(R"({"ws_http_port":12345})");
  ASSERT_EQ(CURLE_OK, webFlag.curlCode);
  EXPECT_TRUE(hasHttpStatus(webFlag, 403)) << webFlag.header;

  auto authFlag = putRaw(R"({"enable_authorize":true})");
  ASSERT_EQ(CURLE_OK, authFlag.curlCode);
  EXPECT_TRUE(hasHttpStatus(authFlag, 403)) << authFlag.header;

  for (const auto* body : {R"({"int32_test":123,"enable_client_white_list":false})",
                           R"({"int32_test":123,"client_white_list":"other"})"}) {
    auto response = putRaw(body);
    ASSERT_EQ(CURLE_OK, response.curlCode);
    EXPECT_TRUE(hasHttpStatus(response, 403)) << response.header;
    EXPECT_EQ(original, FLAGS_int32_test);
  }
}

TEST(FlagsAccessTest, SetFlagsAcceptsScalarValues) {
  gflags::FlagSaver saver;
  auto response =
      putRaw(R"({"bool_test":true,"double_test":2.5,"int32_test":42,"string_test":"updated"})");
  ASSERT_EQ(CURLE_OK, response.curlCode);
  EXPECT_TRUE(hasHttpStatus(response, 200)) << response.header;
  EXPECT_TRUE(FLAGS_bool_test);
  EXPECT_DOUBLE_EQ(2.5, FLAGS_double_test);
  EXPECT_EQ(42, FLAGS_int32_test);
  EXPECT_EQ("updated", FLAGS_string_test);
}

TEST(FlagsAccessTest, SetFlagsRejectsInvalidJsonAndTypes) {
  const std::vector<std::string> invalidBodies = {"",
                                                  "{}",
                                                  "null",
                                                  "[]",
                                                  R"({"int32_test":null})",
                                                  R"({"int32_test":[1]})",
                                                  R"({"int32_test":{"value":1}})",
                                                  R"({"int32_test":)"};
  for (const auto& body : invalidBodies) {
    auto response = putRaw(body);
    ASSERT_EQ(CURLE_OK, response.curlCode);
    EXPECT_TRUE(hasHttpStatus(response, 400)) << "body=" << body << " header=" << response.header;
  }
}

TEST(FlagsAccessTest, SetFlagsRejectsOversizedBody) {
  auto response = putRaw(R"({"string_test":")" + std::string(64 * 1024, 'a') + R"("})");
  ASSERT_EQ(CURLE_OK, response.curlCode);
  EXPECT_TRUE(hasHttpStatus(response, 413)) << response.header;
}

TEST(FlagsAccessTest, JsonTest) {
  std::string resp;
  ASSERT_TRUE(getUrl("/flags?flags=double_test&format=json", resp));
  auto json = folly::parseJson(resp)["flags"];
  ASSERT_TRUE(json.isArray());
  ASSERT_EQ(1UL, json.size());
  ASSERT_TRUE(json[0].isObject());
  ASSERT_EQ(2UL, json[0].size());

  auto it = json[0].find("name");
  ASSERT_NE(json[0].items().end(), it);
  ASSERT_TRUE(it->second.isString());
  EXPECT_EQ("double_test", it->second.getString());

  it = json[0].find("value");
  ASSERT_NE(json[0].items().end(), it);
  ASSERT_TRUE(it->second.isDouble());
  EXPECT_DOUBLE_EQ(FLAGS_double_test, it->second.getDouble());
}

TEST(FlagsAccessTest, VerboseTest) {
  std::string resp;
  ASSERT_TRUE(getUrl("/flags?flags=int32_test&format=json&verbose=true", resp));
  EXPECT_EQ(std::string::npos, resp.find(kHttpAuthTestToken));
  auto json = folly::parseJson(resp)["flags"];
  ASSERT_TRUE(json.isArray());
  ASSERT_EQ(1UL, json.size());
  ASSERT_TRUE(json[0].isObject());
  ASSERT_EQ(7UL, json[0].size());

  auto it = json[0].find("name");
  ASSERT_NE(json[0].items().end(), it);
  ASSERT_TRUE(it->second.isString());
  EXPECT_EQ("int32_test", it->second.getString());

  it = json[0].find("value");
  ASSERT_NE(json[0].items().end(), it);
  ASSERT_TRUE(it->second.isInt());
  EXPECT_EQ(FLAGS_int32_test, it->second.getInt());

  it = json[0].find("type");
  ASSERT_NE(json[0].items().end(), it);
  ASSERT_TRUE(it->second.isString());
  EXPECT_EQ("int32", it->second.getString());

  it = json[0].find("file");
  ASSERT_NE(json[0].items().end(), it);
  ASSERT_TRUE(it->second.isString());
  EXPECT_EQ(__FILE__, it->second.getString());

  it = json[0].find("is_default");
  ASSERT_NE(json[0].items().end(), it);
  ASSERT_TRUE(it->second.isBool());
  EXPECT_TRUE(it->second.getBool());
}

TEST(FlagsAccessTest, ErrorTest) {
  std::string resp;
  ASSERT_TRUE(getUrl("/get_flags123?flags=int32_test", resp));
  EXPECT_TRUE(resp.empty());
}

}  // namespace nebula

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  folly::init(&argc, &argv, true);
  google::SetStderrLogging(google::INFO);

  ::testing::AddGlobalTestEnvironment(new nebula::FlagsAccessTestEnv());

  return RUN_ALL_TESTS();
}

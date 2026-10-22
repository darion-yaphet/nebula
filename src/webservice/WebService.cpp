/* Copyright (c) 2018 vesoft inc. All rights reserved.
 *
 * This source code is licensed under Apache 2.0 License.
 */

#include "webservice/WebService.h"

#include <fcntl.h>
#include <proxygen/httpserver/HTTPServer.h>
#include <proxygen/httpserver/HTTPServerOptions.h>
#include <proxygen/httpserver/RequestHandlerFactory.h>
#include <proxygen/httpserver/ResponseBuilder.h>
#include <sys/stat.h>
#include <unistd.h>

#include <cstdio>

#include "common/thread/NamedThread.h"
#include "webservice/GetFlagsHandler.h"
#include "webservice/GetStatsHandler.h"
#include "webservice/HttpAccessControl.h"
#include "webservice/NotFoundHandler.h"
#include "webservice/Router.h"
#include "webservice/SetFlagsHandler.h"
#include "webservice/StatusHandler.h"

DEFINE_int32(ws_http_port, 11000, "Port to listen on with HTTP protocol");
DEFINE_string(ws_ip, "0.0.0.0", "IP/Hostname to bind to");
DEFINE_int32(ws_threads, 4, "Number of threads for the web service.");
DEFINE_string(ws_http_token_file,
              "",
              "Bearer token file for management HTTP; empty preserves anonymous access. "
              "Loaded at startup; use a private network or TLS proxy.");

#ifdef BUILD_STANDALONE
DEFINE_int32(ws_storage_http_port, 12000, "Port to listen on Storage with HTTP protocol");
DEFINE_int32(ws_storage_threads, 4, "Number of threads for the web service.");
#endif

namespace nebula {
namespace {

class AccessDeniedHandler final : public proxygen::RequestHandler {
 public:
  void onRequest(std::unique_ptr<proxygen::HTTPMessage>) noexcept override {}
  void onBody(std::unique_ptr<folly::IOBuf>) noexcept override {}
  void onEOM() noexcept override {
    proxygen::ResponseBuilder(downstream_)
        .status(401, "Unauthorized")
        .header("WWW-Authenticate", "Bearer realm=\"nebula-management\"")
        .sendWithEOM();
  }
  void onUpgrade(proxygen::UpgradeProtocol) noexcept override {}
  void requestComplete() noexcept override {
    delete this;
  }
  void onError(proxygen::ProxygenError) noexcept override {
    delete this;
  }
};

class WebServiceHandlerFactory : public proxygen::RequestHandlerFactory {
 public:
  WebServiceHandlerFactory(const web::Router* router,
                           std::shared_ptr<const HttpAccessControl> access)
      : router_(router), access_(std::move(access)) {}

  void onServerStart(folly::EventBase*) noexcept override {}

  void onServerStop() noexcept override {}

  proxygen::RequestHandler* onRequest(proxygen::RequestHandler*,
                                      proxygen::HTTPMessage* msg) noexcept override {
    if (!router_->isPublic(msg) &&
        !access_->authorized(msg->getHeaders().getSingleOrEmpty("Authorization"))) {
      return new AccessDeniedHandler();
    }
    return router_->dispatch(msg);
  }

 private:
  const web::Router* router_;
  const std::shared_ptr<const HttpAccessControl> access_;
};

}  // anonymous namespace

using folly::SocketAddress;
using proxygen::HTTPServer;
using proxygen::HTTPServerOptions;

WebService::WebService(const std::string& name) {
  router_ = std::make_unique<web::Router>(name, this);
}

WebService::~WebService() {
  if (server_) {
    server_->stop();
  }
  if (wsThread_) {
    wsThread_->join();
  }
}

Status WebService::start(uint16_t httpPort) {
  if (started_) {
    LOG(INFO) << "Web service has been started.";
    return Status::OK();
  }

  std::string token;
  if (!FLAGS_ws_http_token_file.empty()) {
    int fd = ::open(FLAGS_ws_http_token_file.c_str(), O_RDONLY | O_CLOEXEC | O_NONBLOCK);
    if (fd < 0) {
      return Status::Error("Cannot open management HTTP token file");
    }
    auto* stream = ::fdopen(fd, "rb");
    if (stream == nullptr) {
      ::close(fd);
      return Status::Error("Cannot open management HTTP token stream");
    }
    std::unique_ptr<FILE, decltype(&std::fclose)> input(stream, &std::fclose);
    struct stat info;
    if (::fstat(fd, &info) != 0 || !S_ISREG(info.st_mode) || (info.st_mode & 0077) != 0) {
      return Status::Error("Management HTTP token must be a regular file with private permissions");
    }
    // Bound the read, including an optional final LF or CRLF.
    token.resize(259);
    token.resize(std::fread(token.data(), 1, token.size(), input.get()));
    if (std::ferror(input.get())) {
      return Status::Error("Cannot read management HTTP token file");
    }
    if (!token.empty() && token.back() == '\n') {
      token.pop_back();
      if (!token.empty() && token.back() == '\r') {
        token.pop_back();
      }
    }
    if (!HttpAccessControl::validToken(token)) {
      return Status::Error("Management HTTP token must contain 32-256 bearer-token characters");
    }
  }
  auto access = std::make_shared<const HttpAccessControl>(token);

  router().get("/flags").handler([](web::PathParams&& params) {
    DCHECK(params.empty());
    return new GetFlagsHandler();
  });
  router().put("/flags").handler([access](web::PathParams&& params) {
    DCHECK(params.empty());
    return new SetFlagsHandler(access);
  });
  router().get("/stats").publicHandler([](web::PathParams&& params) {
    DCHECK(params.empty());
    return new GetStatsHandler();
  });
  router().get("/status").publicHandler([](web::PathParams&& params) {
    DCHECK(params.empty());
    return new StatusHandler();
  });
  router().get("/").publicHandler([](web::PathParams&& params) {
    DCHECK(params.empty());
    return new StatusHandler();
  });

  started_ = true;

  std::vector<HTTPServer::IPConfig> ips = {
      {SocketAddress(FLAGS_ws_ip, httpPort, true), HTTPServer::Protocol::HTTP},
  };

  CHECK_GT(FLAGS_ws_threads, 0) << "The number of webservice threads must be greater than zero";

  HTTPServerOptions options;
  options.threads = static_cast<size_t>(FLAGS_ws_threads);
  options.idleTimeout = std::chrono::milliseconds(60000);
  options.enableContentCompression = false;
  options.handlerFactories = proxygen::RequestHandlerChain()
                                 .addThen<WebServiceHandlerFactory>(router_.get(), access)
                                 .build();
  options.h2cEnabled = true;

  server_ = std::make_unique<HTTPServer>(std::move(options));
  server_->bind(ips);

  std::condition_variable cv;
  std::mutex mut;
  auto status = Status::OK();
  bool serverStartedDone = false;

  // Start HTTPServer mainloop in a separate thread
  wsThread_ = std::make_unique<thread::NamedThread>("webservice-listener", [&]() {
    server_->start(
        [&]() {
          auto addresses = server_->addresses();
          CHECK_EQ(addresses.size(), 1UL);
          if (FLAGS_ws_http_port == 0) {
            FLAGS_ws_http_port = addresses[0].address.getPort();
          }
          LOG(INFO) << "Web service started on "
                    << "HTTP[" << FLAGS_ws_http_port << "]";
          {
            std::lock_guard<std::mutex> g(mut);
            serverStartedDone = true;
          }
          cv.notify_all();
        },
        [&](std::exception_ptr eptr) {
          CHECK(eptr);
          try {
            std::rethrow_exception(eptr);
          } catch (const std::exception& e) {
            status = Status::Error("%s", e.what());
          }
          {
            std::lock_guard<std::mutex> g(mut);
            serverStartedDone = true;
          }
          cv.notify_all();
        });
  });

  {
    std::unique_lock<std::mutex> lck(mut);
    cv.wait(lck, [&]() { return serverStartedDone; });

    if (!status.ok()) {
      LOG(ERROR) << "Failed to start web service: " << status;
      return status;
    }
  }
  return status;
}

}  // namespace nebula

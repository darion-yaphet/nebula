/* Copyright (c) 2018 vesoft inc. All rights reserved.
 *
 * This source code is licensed under Apache 2.0 License.
 */

#include "webservice/SetFlagsHandler.h"

#include <folly/Conv.h>
#include <proxygen/httpserver/ResponseBuilder.h>
#include <proxygen/lib/http/ProxygenErrorEnum.h>

#include "common/base/Base.h"
#include "webservice/Common.h"

namespace nebula {

using proxygen::HTTPMessage;
using proxygen::HTTPMethod;
using proxygen::ProxygenError;
using proxygen::ResponseBuilder;
using proxygen::UpgradeProtocol;

void SetFlagsHandler::onRequest(std::unique_ptr<HTTPMessage> headers) noexcept {
  if (!headers->getMethod() || headers->getMethod().value() != HTTPMethod::PUT) {
    // Unsupported method
    err_ = HttpCode::E_UNSUPPORTED_METHOD;
    return;
  }
}

void SetFlagsHandler::onBody(std::unique_ptr<folly::IOBuf> body) noexcept {
  constexpr size_t kMaxBodySize = 64 * 1024;
  if (err_ != HttpCode::SUCCEEDED || !body) {
    return;
  }
  const auto size = body->computeChainDataLength();
  if (size > kMaxBodySize - bodySize_) {
    err_ = HttpCode::E_TOO_LARGE;
    body_.reset();
    return;
  }
  bodySize_ += size;
  if (body_) {
    body_->appendChain(std::move(body));
  } else {
    body_ = std::move(body);
  }
}

void SetFlagsHandler::onEOM() noexcept {
  auto sendError = [this](int code, const char* reason) {
    ResponseBuilder(downstream_)
        .status(code, reason)
        .body(folly::toJson(folly::dynamic::object("errorMessage", reason)))
        .sendWithEOM();
  };
  if (err_ == HttpCode::E_UNSUPPORTED_METHOD) {
    sendError(405, "Method Not Allowed");
    return;
  }
  if (err_ == HttpCode::E_TOO_LARGE) {
    sendError(413, "Payload Too Large");
    return;
  }

  std::vector<std::pair<std::string, std::string>> updates;
  try {
    if (!body_) {
      sendError(400, "Bad Request");
      return;
    }
    auto flags = folly::parseJson(body_->moveToFbString().toStdString());
    if (!flags.isObject() || flags.empty()) {
      sendError(400, "Bad Request");
      return;
    }
    // Validate every name and value before changing any flag. A denied name in
    // a mixed batch must not leave earlier flags modified.
    for (const auto& item : flags.items()) {
      if (!item.first.isString() || !(item.second.isString() || item.second.isBool() ||
                                      item.second.isInt() || item.second.isDouble())) {
        sendError(400, "Bad Request");
        return;
      }
      const auto& name = item.first.getString();
      if (!access_ || !access_->canSetFlag(name)) {
        sendError(403, "Forbidden");
        return;
      }
      auto value = item.second.asString();
      if (value.find('\0') != std::string::npos) {
        sendError(400, "Bad Request");
        return;
      }
      updates.emplace_back(name, std::move(value));
    }
  } catch (const std::exception&) {
    // Do not log request contents or parser errors containing sensitive values.
    sendError(400, "Bad Request");
    return;
  }

  folly::dynamic failedOptions = folly::dynamic::array();
  for (const auto& update : updates) {
    const auto& name = update.first;
    const auto& value = update.second;
    if (gflags::SetCommandLineOption(name.c_str(), value.c_str()).empty()) {
      failedOptions.push_back(name);
    }
  }
  auto reply = failedOptions.empty() ? folly::dynamic::object("errCode", 0)
                                     : folly::dynamic::object("failedOptions", failedOptions);
  ResponseBuilder(downstream_)
      .status(WebServiceUtils::to(HttpStatusCode::OK),
              WebServiceUtils::toString(HttpStatusCode::OK))
      .body(folly::toPrettyJson(reply))
      .sendWithEOM();
}

void SetFlagsHandler::onUpgrade(UpgradeProtocol) noexcept {
  // Do nothing
}

void SetFlagsHandler::requestComplete() noexcept {
  delete this;
}

void SetFlagsHandler::onError(ProxygenError err) noexcept {
  LOG(ERROR) << "Web service SetFlagsHandler got error: " << proxygen::getErrorString(err);
  delete this;
}

}  // namespace nebula

/* Copyright (c) 2018 vesoft inc. All rights reserved.
 *
 * This source code is licensed under Apache 2.0 License.
 */

#ifndef WEBSERVICE_SETFLAGSHANDLER_H_
#define WEBSERVICE_SETFLAGSHANDLER_H_

#include <proxygen/httpserver/RequestHandler.h>

#include "common/base/Base.h"
#include "webservice/Common.h"
#include "webservice/HttpAccessControl.h"

namespace nebula {

class SetFlagsHandler : public proxygen::RequestHandler {
 public:
  explicit SetFlagsHandler(std::shared_ptr<const HttpAccessControl> access)
      : access_(std::move(access)) {}

  void onRequest(std::unique_ptr<proxygen::HTTPMessage> headers) noexcept override;

  void onBody(std::unique_ptr<folly::IOBuf> body) noexcept override;

  void onEOM() noexcept override;

  void onUpgrade(proxygen::UpgradeProtocol proto) noexcept override;

  void requestComplete() noexcept override;

  void onError(proxygen::ProxygenError err) noexcept override;

 private:
  const std::shared_ptr<const HttpAccessControl> access_;
  HttpCode err_{HttpCode::SUCCEEDED};
  std::unique_ptr<folly::IOBuf> body_;
  size_t bodySize_{0};
};

}  // namespace nebula
#endif  // WEBSERVICE_SETFLAGSHANDLER_H_

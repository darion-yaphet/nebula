/* Copyright (c) 2018 vesoft inc. All rights reserved.
 *
 * This source code is licensed under Apache 2.0 License.
 */
#include "daemons/DaemonInit.h"

#include "common/process/ProcessUtils.h"

namespace nebula {

Status initDaemonProcess(const std::string& pidPath, bool daemonize) {
  // 检查 PID 文件是否可用
  auto status = ProcessUtils::isPidAvailable(pidPath);
  if (!status.ok()) {
    return status;
  }

  if (daemonize) {
    // 后台化进程
    status = ProcessUtils::daemonize(pidPath);
    if (!status.ok()) {
      return status;
    }
  } else {
    // 前台运行，仍需创建 PID 文件
    status = ProcessUtils::makePidFile(pidPath);
    if (!status.ok()) {
      return status;
    }
  }

  return Status::OK();
}

}  // namespace nebula

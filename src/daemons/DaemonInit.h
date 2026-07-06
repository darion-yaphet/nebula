/* Copyright (c) 2018 vesoft inc. All rights reserved.
 *
 * This source code is licensed under Apache 2.0 License.
 */
#ifndef DAEMONS_DAEMONINIT_H
#define DAEMONS_DAEMONINIT_H

#include "common/base/SignalHandler.h"
#include "common/base/Status.h"

namespace nebula {

/**
 * @brief 初始化守护进程的 PID 文件和进程状态
 *
 * 检查 PID 文件是否可用，根据 daemonize 标志决定是否后台化进程，
 * 并创建 PID 文件。
 *
 * @param pidPath PID 文件路径
 * @param daemonize 是否后台化进程
 * @return Status 操作状态
 */
Status initDaemonProcess(const std::string& pidPath, bool daemonize);

/**
 * @brief 信号处理回调函数模板
 *
 * @tparam ServerType 服务器类型（必须有 stop() 或 notifyStop() 方法）
 */
template <typename ServerType>
void signalHandlerImpl(ServerType* server, int sig) {
  switch (sig) {
    case SIGINT:
    case SIGTERM:
      FLOG_INFO("Signal %d(%s) received, stopping this server", sig, ::strsignal(sig));
      if (server) {
        // 尝试调用 notifyStop()，如果不存在则调用 stop()
        if constexpr (requires { server->notifyStop(); }) {
          server->notifyStop();
        } else {
          server->stop();
        }
      }
      break;
    default:
      FLOG_ERROR("Signal %d(%s) received but ignored", sig, ::strsignal(sig));
  }
}

/**
 * @brief 设置信号处理器
 *
 * @tparam ServerType 服务器类型
 * @param server 服务器实例指针
 * @return Status 操作状态
 */
template <typename ServerType>
Status setupSignalHandler(ServerType* server) {
  return SignalHandler::install(
      {SIGINT, SIGTERM}, [server](SignalHandler::GeneralSignalInfo* info) {
        signalHandlerImpl(server, info->sig());
      });
}

}  // namespace nebula

#endif  // DAEMONS_DAEMONINIT_H

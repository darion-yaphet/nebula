# NebulaGraph

[English](README.md) | 简体中文

NebulaGraph 是一款开源的分布式图数据库，用于存储和查询相互关联的数据。它使用属性图模型，以点和边及其属性表示数据；查询语言 nGQL 支持图遍历和模式匹配。常见应用包括知识图谱、推荐、反欺诈和网络分析。

## 架构

NebulaGraph 将查询处理与数据存储分离。一次部署通常包含三个服务：

- **Graph 服务（`graphd`）**接收客户端查询，生成并执行查询计划，返回结果。
- **Storage 服务（`storaged`）**按分区存储图数据并处理读写，副本通过 Raft 达成共识。
- **Meta 服务（`metad`）**管理集群元数据、Schema 和分区分布。

具体说明请参阅[架构总览](https://docs.nebula-graph.com.cn/3.8.0/1.introduction/3.nebula-graph-architecture/1.architecture-overview/)。

## 快速开始

本地体验请参考 [Docker 快速入门](https://docs.nebula-graph.com.cn/3.8.0/2.quick-start/1.quick-start-overview/)，其中包含环境要求、部署和连接步骤。部署文件维护在独立的 [nebula-docker-compose](https://github.com/vesoft-inc/nebula-docker-compose) 仓库中。

从本仓库编译服务，请参考[源码编译指南](https://docs.nebula-graph.com.cn/3.8.0/4.deployment-and-installation/2.compile-and-install-nebula-graph/1.install-nebula-graph-by-compiling-the-source-code/)。本项目的 CMake 配置支持 Linux；指南介绍了依赖、编译选项和安装步骤。请根据部署版本选择相匹配的文档和客户端。

连接服务后，可以通过 [nGQL 命令汇总](https://docs.nebula-graph.com.cn/3.8.0/2.quick-start/6.cheatsheet-for-ngql-command/)创建图空间并执行查询。[NebulaGraph Console](https://github.com/vesoft-inc/nebula-console) 是独立的命令行客户端；其他语言客户端请参阅[客户端列表](src/clients/graph/README.md)。

## 仓库目录

| 路径 | 内容 |
| --- | --- |
| [`src/graph`](src/graph) | 查询规划与执行 |
| [`src/meta`](src/meta) | 元数据服务 |
| [`src/storage`](src/storage) | 存储服务 |
| [`src/kvstore`](src/kvstore) | 键值存储与复制 |
| [`src/clients`](src/clients) | 内部 Meta 和 Storage 客户端 |
| [`src/interface`](src/interface) | Thrift 服务定义 |
| [`conf`](conf) | 服务配置模板 |
| [`tests`](tests) 和各服务的 `test` 目录 | 集成测试与单元测试 |

[服务脚本指南](scripts/README.md)介绍已安装服务的启动和停止方式。本仓库的管理 HTTP 接口认证选项请参阅 [Management HTTP access](docs/management-http.md)（英文）。

CMake 配置默认启用单元测试。集成测试的环境要求和运行命令请参阅[集成测试指南](tests/README.md)（英文）。

## 文档与社区

- [NebulaGraph 中文文档](https://docs.nebula-graph.com.cn/3.8.0/)
- [报告问题或提出功能建议](https://github.com/vesoft-inc/nebula/issues)
- [GitHub Discussions](https://github.com/vesoft-inc/nebula/discussions)
- [中文社区论坛](https://discuss.nebula-graph.com.cn/)

## 参与贡献

欢迎参与贡献。请参阅[贡献指南](CONTRIBUTING.md)和[代码风格指南](Coding_Style_Guide.md)。如需报告安全问题，请遵循[安全政策](.github/Security.md)。

## 许可证

NebulaGraph 采用 [Apache License 2.0](LICENSE) 许可证。

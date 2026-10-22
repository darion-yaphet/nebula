# NebulaGraph

English | [简体中文](README-CN.md)

NebulaGraph is an open-source, distributed graph database for storing and querying connected data. Its property graph model represents data as vertices and edges with properties, and its nGQL query language supports graph traversal and pattern matching. Common uses include knowledge graphs, recommendations, fraud detection, and network analysis.

## Architecture

NebulaGraph separates query processing from storage. A deployment typically runs three services:

- **Graph Service (`graphd`)** accepts client queries, plans and executes them, and returns results.
- **Storage Service (`storaged`)** stores graph data in partitions and serves reads and writes. Replicas use Raft for consensus.
- **Meta Service (`metad`)** manages cluster metadata, schemas, and partition placement.

See the [architecture overview](https://docs.nebula-graph.io/3.8.0/1.introduction/3.nebula-graph-architecture/1.architecture-overview/) for details.

## Get started

To try NebulaGraph locally, follow the [Docker quick start](https://docs.nebula-graph.io/3.8.0/2.quick-start/1.quick-start-workflow/). It covers prerequisites, deployment, and connecting to the services. The deployment files are maintained in the separate [nebula-docker-compose](https://github.com/vesoft-inc/nebula-docker-compose) repository.

To build the server from this repository, use the [source build guide](https://docs.nebula-graph.io/3.8.0/4.deployment-and-installation/2.compile-and-install-nebula-graph/1.install-nebula-graph-by-compiling-the-source-code/). The CMake configuration supports Linux; the guide describes the required dependencies, build options, and installation steps. Choose documentation and client versions that match your deployment.

Once connected, use the [nGQL cheatsheet](https://docs.nebula-graph.io/3.8.0/2.quick-start/6.cheatsheet-for-ngql/) to create a graph space and run queries. [NebulaGraph Console](https://github.com/vesoft-inc/nebula-console) is a separate command-line client; other language clients are listed in [the client overview](src/clients/graph/README.md).

## Repository layout

| Path | Contents |
| --- | --- |
| [`src/graph`](src/graph) | Query planning and execution |
| [`src/meta`](src/meta) | Metadata service |
| [`src/storage`](src/storage) | Storage service |
| [`src/kvstore`](src/kvstore) | Key-value storage and replication |
| [`src/clients`](src/clients) | Internal Meta and Storage clients |
| [`src/interface`](src/interface) | Thrift service definitions |
| [`conf`](conf) | Service configuration templates |
| [`tests`](tests) and service-specific `test` directories | Integration and unit tests |

The [service scripts guide](scripts/README.md) describes starting and stopping installed services. For this repository's management HTTP authentication options, see [Management HTTP access](docs/management-http.md).

Unit tests are enabled by default in the CMake configuration. For integration test prerequisites and commands, see the [integration test guide](tests/README.md).

## Documentation and community

- [NebulaGraph documentation](https://docs.nebula-graph.io/3.8.0/)
- [Report a bug or request a feature](https://github.com/vesoft-inc/nebula/issues)
- [Ask a question or join a discussion](https://github.com/vesoft-inc/nebula/discussions)
- [Chinese community forum](https://discuss.nebula-graph.com.cn/)

## Contributing

Contributions are welcome. See [CONTRIBUTING.md](CONTRIBUTING.md) for contribution guidance and [Coding_Style_Guide.md](Coding_Style_Guide.md) for formatting conventions. For security reports, follow the [security policy](.github/Security.md).

## License

NebulaGraph is licensed under the [Apache License 2.0](LICENSE).

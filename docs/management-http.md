# Management HTTP access

Graph, Meta and Storage share the same optional HTTP authentication policy.
When `ws_http_token_file` is empty (the default), existing clients can continue
to access management routes without credentials, including `GET/PUT /flags`,
Storage's `/admin` and `/rocksdb_property`, and Meta's `/replace`.

Configuring a token file enables authentication for management routes, including
custom routes. `GET /`, `/status`, `/stats` and `/rocksdb_stats` remain public, so
health checks and metric collection continue to work without credentials.

## Enable authenticated access

Create a random token file outside the repository, readable only by the service
account. For example, run this as that account:

```sh
umask 077
openssl rand -hex 32 > /secure/path/management.token
```

Configure each daemon that needs authenticated management access:

```text
--ws_http_token_file=/secure/path/management.token
```

The file must contain one 32-256 character bearer token, with an optional final
LF or CRLF. It must be owned by the daemon's effective user and be a regular
file with no group/other permissions (for
example mode `0600` or `0400`). A missing, unreadable, empty, malformed or
insecurely permissioned configured file prevents HTTP startup.
The token is loaded once at startup;
changing the file or gflags later does not change the running access policy.
Restart the daemon to rotate its token.
The token itself is not a gflag and is not exposed by `GET /flags`.

Once a token file is configured, clients must send `Authorization: Bearer <token>`
on every management request.
Missing, incorrect or repeated Authorization headers return HTTP 401. Credentials
in query parameters are not accepted. This policy is independent of Graph's
`enable_authorize` setting.

These listeners use plain HTTP. Bind `ws_ip` to a restricted management interface
and enforce network access controls, or place them behind a TLS proxy that
preserves the Authorization header. Enabling RPC SSL does not enable HTTPS on
these listeners. Update existing scripts calling `/flags`, `/admin`, `/replace`
or `/rocksdb_property` to supply the header before enabling authentication.

## Changing flags

Clients can update ordinary flags without configuring a per-flag allowlist;
authentication is required only when a token file is configured. Security flags
(including authentication, SSL credentials, UDF loading and every `ws_` flag)
cannot be changed through this API.

`PUT /flags` accepts a nonempty JSON object of scalar values, with a maximum body
size of 64 KiB. Invalid JSON or value types return 400, disallowed names return
403, and oversized bodies return 413. The entire object is checked before any
flag is changed. Existing `failedOptions` responses are retained for ordinary
flags that gflags cannot set. Updates are not transactional when an individual
gflags validator rejects a value; earlier valid changes may already have applied.

This change protects access to management operations. It does not add per-user
roles or TLS to the HTTP server. Possession of the token grants access to all
management routes, subject to the fixed restrictions on security flag changes.

# Deployment Notes

## Local Ports

- Client connects to HAProxy at `127.0.0.1:7000`.
- HAProxy forwards TCP connections to Qt server instances:
  - `127.0.0.1:7101`
  - `127.0.0.1:7102`
  - `127.0.0.1:7103`

## Server Configuration

The server reads command line options or environment variables:

- `SERVER_LISTEN_ADDRESS`
- `SERVER_LISTEN_PORT`
- `DB_DRIVER`
- `DB_HOST`
- `DB_PORT`
- `DB_USER`
- `DB_PASSWORD`
- `DB_NAME`
- `DB_WORKERS`

This workspace now includes a built `QMYSQL` plugin for the current Qt 5.14.0 MinGW 32-bit kit. Keep `qsqlmysql.dll` in a `sqldrivers` plugin folder and keep `libmysql.dll` beside the server executable or in the runtime `PATH`.

Example:

```powershell
$env:SERVER_LISTEN_PORT="7101"
$env:DB_HOST="127.0.0.1"
$env:DB_USER="root"
$env:DB_PASSWORD="password"
.\QtDatabaseServer.exe
```

## HAProxy

Use `haproxy.cfg` as the TCP load balancer config. For production-scale concurrent users, also tune OS socket limits, HAProxy `maxconn`, server instance count, and MySQL capacity.

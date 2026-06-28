# Server-Client Database Sample

Qt 5.14.0 server-client database browser/editor sample.

## Projects

- `Common`: shared length-prefixed JSON TCP protocol.
- `Server`: Qt `QCoreApplication` TCP server. The server owns all MySQL access.
- `Client`: Qt Widgets client. The client talks only to the server or load balancer.
- `Deploy`: HAProxy and server instance configuration examples.

## Build

From a fresh clone on this machine, build and deploy local runtime files with:

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\build.ps1
```

Default expected local tools:

- Qt: `C:\Qt\Qt5.14.0\5.14.0\mingw73_32`
- MinGW: `C:\Qt\Qt5.14.0\Tools\mingw730_32`

To use different paths:

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\build.ps1 `
  -QtRoot "C:\Qt\Qt5.14.0\5.14.0\mingw73_32" `
  -MingwRoot "C:\Qt\Qt5.14.0\Tools\mingw730_32"
```

## Run Locally

Start XAMPP MariaDB if needed, start the Qt server on port `7000`, and open the client:

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\run-local.ps1
```

Stop the local Qt server/client:

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\stop-local.ps1
```

Add `-StopMariaDb` if you also want to stop the XAMPP MariaDB process started for local testing.

## Notes

- The client never connects to MySQL directly.
- Table content is loaded with pagination.
- Insert, update, and delete are validated on the server.
- Existing-row update and delete require primary key values.
- For production scale, run multiple server instances behind `Deploy/haproxy.cfg` and tune MySQL, OS socket limits, HAProxy limits, and worker counts.

## MySQL Driver Requirement

The repository includes the small runtime pieces needed for MySQL access without downloading after clone:

- `Runtime\sqldrivers\qsqlmysql.dll`
- `Runtime\mysql\libmysql.dll`
- minimal MySQL Connector/C headers and import library under `ThirdParty\mysql-connector-c-6.1.11-win32`

The build script copies those into the `Server` folder. Use `.\scripts\build.ps1 -RebuildSqlPlugin` only if you need to rebuild `qsqlmysql.dll` from source.

## Git Policy

Source, deployment scripts, documentation, the MySQL runtime DLL, and the prebuilt Qt MySQL SQL plugin are versioned.

Generated files are ignored:

- app executables
- qmake Makefiles and object files
- logs under `Build`
- copied Qt runtime DLLs under `Client` and `Server`

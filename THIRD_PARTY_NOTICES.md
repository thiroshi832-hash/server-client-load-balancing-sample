# Third-Party Notices

This repository includes small runtime/build artifacts so a fresh local clone can build and run without downloading dependencies.

## Qt

The application targets Qt 5.14.0 MinGW 32-bit. Qt itself is not vendored in this repository. The build script copies required Qt runtime DLLs from the local Qt installation into `Client` and `Server` after building.

Default expected paths:

- `C:\Qt\Qt5.14.0\5.14.0\mingw73_32`
- `C:\Qt\Qt5.14.0\Tools\mingw730_32`

Qt license information is provided by The Qt Company with the installed Qt distribution.

## Qt MySQL SQL Plugin

`Runtime\sqldrivers\qsqlmysql.dll` is a Qt 5.14.0 MinGW 32-bit MySQL SQL driver plugin built from the local Qt source tree.

The rebuild project is in `Tools\qsqlmysql`.

## MySQL Connector/C

`Runtime\mysql\libmysql.dll` and the minimal rebuild files under `ThirdParty\mysql-connector-c-6.1.11-win32` come from Oracle MySQL Connector/C 6.1.11 for Windows 32-bit.

The original Connector/C license file is retained at:

`ThirdParty\mysql-connector-c-6.1.11-win32\COPYING`

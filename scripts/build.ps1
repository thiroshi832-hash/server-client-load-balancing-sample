param(
    [string]$QtRoot = "C:\Qt\Qt5.14.0\5.14.0\mingw73_32",
    [string]$MingwRoot = "C:\Qt\Qt5.14.0\Tools\mingw730_32",
    [switch]$RebuildSqlPlugin
)

$ErrorActionPreference = "Stop"

$RepoRoot = Split-Path -Parent $PSScriptRoot
$Qmake = Join-Path $QtRoot "bin\qmake.exe"
$WinDeployQt = Join-Path $QtRoot "bin\windeployqt.exe"
$Make = Join-Path $MingwRoot "bin\mingw32-make.exe"
$QtBin = Join-Path $QtRoot "bin"
$QtPlugins = Join-Path $QtRoot "plugins"

function Require-File($Path, $Message) {
    if (-not (Test-Path -LiteralPath $Path)) {
        throw "$Message Missing: $Path"
    }
}

function Copy-RequiredFile($Source, $Destination) {
    Require-File $Source "Required file not found."
    $parent = Split-Path -Parent $Destination
    New-Item -ItemType Directory -Force -Path $parent | Out-Null
    Copy-Item -LiteralPath $Source -Destination $Destination -Force
}

Require-File $Qmake "Qt qmake was not found."
Require-File $Make "MinGW make was not found."

if ($RebuildSqlPlugin) {
    $PluginBuildDir = Join-Path $RepoRoot "Build\qsqlmysql-local"
    New-Item -ItemType Directory -Force -Path $PluginBuildDir | Out-Null
    Push-Location $PluginBuildDir
    try {
        & $Qmake (Join-Path $RepoRoot "Tools\qsqlmysql\qsqlmysql.pro") -spec win32-g++ CONFIG+=release CONFIG-=debug
        if ($LASTEXITCODE -ne 0) { throw "qmake failed for qsqlmysql plugin." }
        & $Make
        if ($LASTEXITCODE -ne 0) { throw "Build failed for qsqlmysql plugin." }
    } finally {
        Pop-Location
    }

    Copy-RequiredFile `
        (Join-Path $RepoRoot "Build\qsqlmysql-plugin\qsqlmysql.dll") `
        (Join-Path $RepoRoot "Runtime\sqldrivers\qsqlmysql.dll")
}

foreach ($Project in @("Server", "Client")) {
    Push-Location (Join-Path $RepoRoot $Project)
    try {
        & $Qmake "$Project.pro" -spec win32-g++ CONFIG+=release CONFIG-=debug
        if ($LASTEXITCODE -ne 0) { throw "qmake failed for $Project." }
        & $Make
        if ($LASTEXITCODE -ne 0) { throw "Build failed for $Project." }
    } finally {
        Pop-Location
    }
}

$ClientDir = Join-Path $RepoRoot "Client"
$ServerDir = Join-Path $RepoRoot "Server"

foreach ($Dll in @("Qt5Core.dll", "Qt5Gui.dll", "Qt5Widgets.dll", "Qt5Network.dll", "libgcc_s_dw2-1.dll", "libstdc++-6.dll", "libwinpthread-1.dll")) {
    Copy-RequiredFile (Join-Path $QtBin $Dll) (Join-Path $ClientDir $Dll)
}
Copy-RequiredFile (Join-Path $QtPlugins "platforms\qwindows.dll") (Join-Path $ClientDir "platforms\qwindows.dll")

foreach ($Dll in @("Qt5Core.dll", "Qt5Network.dll", "Qt5Sql.dll", "libgcc_s_dw2-1.dll", "libstdc++-6.dll", "libwinpthread-1.dll")) {
    Copy-RequiredFile (Join-Path $QtBin $Dll) (Join-Path $ServerDir $Dll)
}
Copy-RequiredFile (Join-Path $RepoRoot "Runtime\mysql\libmysql.dll") (Join-Path $ServerDir "libmysql.dll")
Copy-RequiredFile (Join-Path $RepoRoot "Runtime\sqldrivers\qsqlmysql.dll") (Join-Path $ServerDir "sqldrivers\qsqlmysql.dll")

Write-Host "Build complete."
Write-Host "Server: $ServerDir\QtDatabaseServer.exe"
Write-Host "Client: $ClientDir\QtDatabaseClient.exe"

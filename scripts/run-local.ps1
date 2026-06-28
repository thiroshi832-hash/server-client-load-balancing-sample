param(
    [int]$ServerPort = 7000,
    [string]$DbHost = "127.0.0.1",
    [int]$DbPort = 3306,
    [string]$DbUser = "root",
    [string]$DbPassword = "",
    [string]$XamppMysqlRoot = "C:\xampp\mysql",
    [switch]$NoClient
)

$ErrorActionPreference = "Stop"

$RepoRoot = Split-Path -Parent $PSScriptRoot
$BuildLogDir = Join-Path $RepoRoot "Build\run-logs"
New-Item -ItemType Directory -Force -Path $BuildLogDir | Out-Null

$ServerExe = Join-Path $RepoRoot "Server\QtDatabaseServer.exe"
$ClientExe = Join-Path $RepoRoot "Client\QtDatabaseClient.exe"

if (-not (Test-Path -LiteralPath $ServerExe) -or -not (Test-Path -LiteralPath $ClientExe)) {
    throw "Build first: powershell -ExecutionPolicy Bypass -File scripts\build.ps1"
}

$MysqlAdmin = Join-Path $XamppMysqlRoot "bin\mysqladmin.exe"
$Mysqld = Join-Path $XamppMysqlRoot "bin\mysqld.exe"
$MysqlIni = Join-Path $XamppMysqlRoot "bin\my.ini"

if (-not (Get-NetTCPConnection -LocalPort $DbPort -ErrorAction SilentlyContinue | Where-Object { $_.State -eq "Listen" })) {
    if (Test-Path -LiteralPath $Mysqld) {
        Start-Process `
            -FilePath $Mysqld `
            -ArgumentList "--defaults-file=$MysqlIni", "--standalone" `
            -WindowStyle Hidden `
            -RedirectStandardOutput (Join-Path $BuildLogDir "mysqld.out.log") `
            -RedirectStandardError (Join-Path $BuildLogDir "mysqld.err.log") | Out-Null
    } else {
        throw "No database is listening on port $DbPort, and XAMPP MariaDB was not found at $XamppMysqlRoot."
    }
}

if (Test-Path -LiteralPath $MysqlAdmin) {
    $ready = $false
    for ($i = 0; $i -lt 30; $i++) {
        & $MysqlAdmin --protocol=tcp --host=$DbHost --port=$DbPort --user=$DbUser ping *> $null
        if ($LASTEXITCODE -eq 0) { $ready = $true; break }
        Start-Sleep -Milliseconds 500
    }
    if (-not $ready) {
        throw "Database did not become ready on ${DbHost}:${DbPort}."
    }
}

$existing = Get-NetTCPConnection -LocalPort $ServerPort -ErrorAction SilentlyContinue |
    Where-Object { $_.State -eq "Listen" } |
    Select-Object -First 1

if ($existing) {
    $process = Get-Process -Id $existing.OwningProcess -ErrorAction SilentlyContinue
    if ($process -and $process.Path -eq $ServerExe) {
        Stop-Process -Id $process.Id -Force
        Start-Sleep -Milliseconds 500
    } elseif ($process) {
        throw "Port $ServerPort is already used by $($process.ProcessName) pid $($process.Id)."
    }
}

$serverArgs = @(
    "--listen-port", "$ServerPort",
    "--workers", "4",
    "--db-driver", "QMYSQL",
    "--db-host", $DbHost,
    "--db-port", "$DbPort",
    "--db-user", $DbUser
)
if ($DbPassword.Length -gt 0) {
    $serverArgs += @("--db-password", $DbPassword)
}

$server = Start-Process `
    -FilePath $ServerExe `
    -WorkingDirectory (Join-Path $RepoRoot "Server") `
    -ArgumentList $serverArgs `
    -PassThru `
    -WindowStyle Hidden `
    -RedirectStandardOutput (Join-Path $BuildLogDir "qt-server.out.log") `
    -RedirectStandardError (Join-Path $BuildLogDir "qt-server.err.log")

Start-Sleep -Seconds 1
if ($server.HasExited) {
    throw "Qt server exited early with code $($server.ExitCode). See $BuildLogDir."
}

if (-not $NoClient) {
    $client = Start-Process `
        -FilePath $ClientExe `
        -WorkingDirectory (Join-Path $RepoRoot "Client") `
        -ArgumentList "--server-host", "127.0.0.1", "--server-port", "$ServerPort" `
        -PassThru
}

[pscustomobject]@{
    ServerPid = $server.Id
    ServerPort = $ServerPort
    ClientPid = if ($NoClient) { $null } else { $client.Id }
    Logs = $BuildLogDir
} | Format-List

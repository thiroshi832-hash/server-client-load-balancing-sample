param(
    [switch]$StopMariaDb
)

$ErrorActionPreference = "Continue"
$RepoRoot = Split-Path -Parent $PSScriptRoot

$ServerExe = Join-Path $RepoRoot "Server\QtDatabaseServer.exe"
$ClientExe = Join-Path $RepoRoot "Client\QtDatabaseClient.exe"

Get-Process | Where-Object {
    ($_.ProcessName -eq "QtDatabaseServer" -and $_.Path -eq $ServerExe) -or
    ($_.ProcessName -eq "QtDatabaseClient" -and $_.Path -eq $ClientExe)
} | Stop-Process -Force

if ($StopMariaDb) {
    Get-Process | Where-Object {
        $_.ProcessName -eq "mysqld" -and $_.Path -like "C:\xampp\mysql\bin\mysqld.exe"
    } | Stop-Process -Force
}

Write-Host "Stopped local Qt server/client processes."

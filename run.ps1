$ErrorActionPreference = "Stop"
$Root = Split-Path -Parent $MyInvocation.MyCommand.Path

if (-not [string]::IsNullOrWhiteSpace($env:QT_RUNTIME_DIR)) {
    $env:PATH = "$env:QT_RUNTIME_DIR;$env:PATH"
}

$exe = Join-Path $Root "build\ClientOpsStudio.exe"
if (!(Test-Path $exe)) {
    Write-Error "未找到可执行文件: $exe，请先运行 build.ps1"
}

& $exe

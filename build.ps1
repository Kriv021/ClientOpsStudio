param(
    [string]$BuildType = "Release",
    [string]$Generator = "",
    [string]$QtPrefixPath = "",
    [string]$CxxCompiler = "",
    [string]$MakeProgram = ""
)

$ErrorActionPreference = "Stop"
$Root = Split-Path -Parent $MyInvocation.MyCommand.Path
$BuildDir = Join-Path $Root "build"

if ([string]::IsNullOrWhiteSpace($QtPrefixPath)) {
    $QtPrefixPath = $env:QT_ROOT
}

if ([string]::IsNullOrWhiteSpace($CxxCompiler)) {
    $CxxCompiler = $env:CXX
}

if ([string]::IsNullOrWhiteSpace($MakeProgram)) {
    $MakeProgram = $env:CMAKE_MAKE_PROGRAM
}

$configureArgs = @("-S", $Root, "-B", $BuildDir)

if (-not [string]::IsNullOrWhiteSpace($Generator)) {
    $configureArgs += @("-G", $Generator)
}
if (-not [string]::IsNullOrWhiteSpace($QtPrefixPath)) {
    $configureArgs += "-DCMAKE_PREFIX_PATH=$QtPrefixPath"
}
if (-not [string]::IsNullOrWhiteSpace($CxxCompiler)) {
    $configureArgs += "-DCMAKE_CXX_COMPILER=$CxxCompiler"
}
if (-not [string]::IsNullOrWhiteSpace($MakeProgram)) {
    $configureArgs += "-DCMAKE_MAKE_PROGRAM=$MakeProgram"
}

cmake @configureArgs
cmake --build $BuildDir --config $BuildType -j 8

Write-Host "Build success: $BuildDir"

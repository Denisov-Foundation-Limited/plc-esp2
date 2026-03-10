param(
    [string]$Environment = "fcplc",
    [switch]$Clean,
    [switch]$SingleCore
)

$ErrorActionPreference = "Stop"

$projectRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$pioExe = Join-Path $projectRoot ".venv-pio\Scripts\pio.exe"

if (!(Test-Path $pioExe)) {
    Write-Error "PlatformIO not found at $pioExe. Create venv first."
}

$args = @("run", "-e", $Environment)
if ($Clean) {
    $args += "-t"
    $args += "clean"
}

Write-Host "Using PlatformIO: $pioExe"
if ($SingleCore) {
    $argsSingle = @($args + @("-j", "1"))
    Write-Host "Command: pio $($argsSingle -join ' ')"
    & $pioExe @argsSingle
    exit $LASTEXITCODE
}

$jobs = [Environment]::ProcessorCount
if ($jobs -lt 1) { $jobs = 1 }
$argsMulti = @($args + @("-j", "$jobs"))

Write-Host "Command: pio $($argsMulti -join ' ')"
& $pioExe @argsMulti
if ($LASTEXITCODE -eq 0 -or $jobs -eq 1) {
    exit $LASTEXITCODE
}

Write-Host "Build failed, retrying with single core (-j 1)..."
$argsFallback = @($args + @("-j", "1"))
Write-Host "Command: pio $($argsFallback -join ' ')"
& $pioExe @argsFallback
exit $LASTEXITCODE

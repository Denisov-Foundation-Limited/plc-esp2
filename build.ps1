param(
    [string]$Environment = "fcplc",
    [switch]$Clean
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
Write-Host "Command: pio $($args -join ' ')"

& $pioExe @args
exit $LASTEXITCODE

[CmdletBinding()]
param()

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$repoRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
$outputPath = Join-Path $repoRoot '.pio\firmware_random_host.exe'

Push-Location $repoRoot
try {
    & g++ -std=c++17 -Iinclude `
        test/firmware_random/test_main.cpp `
        src/common/FirmwareRandom.cpp -o $outputPath
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    & $outputPath
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    Write-Host '[PASS] Firmware random: preserved 16-bit sequence; 32-bit bounds, deterministic seed, zero cases and rejection tail'
    exit 0
}
finally {
    Pop-Location
}

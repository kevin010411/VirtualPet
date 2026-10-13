[CmdletBinding()]
param()

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$repoRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
$outputPath = Join-Path $repoRoot '.pio\pet_persistence_host.exe'

Push-Location $repoRoot
try {
    foreach ($deathEnabled in @(0, 1)) {
        & g++ -std=c++17 "-DENABLE_DEATH=$deathEnabled" -Itest/host_stubs -Iinclude `
            test/pet_persistence/test_main.cpp `
            src/pet/Pet.cpp `
            -o $outputPath
        if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
        & $outputPath
        if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
        Write-Host "[PASS] Death gating, saved Dead and new-life reset (ENABLE_DEATH=$deathEnabled)"
    }
    exit 0
}
finally {
    Pop-Location
}

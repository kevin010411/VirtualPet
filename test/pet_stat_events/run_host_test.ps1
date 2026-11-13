[CmdletBinding()]
param()

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$repoRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
Push-Location $repoRoot
try {
    New-Item -ItemType Directory -Force .pio | Out-Null
    $outputPath = '.pio/pet_stat_events_host.exe'
    & g++ -std=c++17 -Wall -Wextra -DENABLE_STAT_EVENTS=1 `
        -Itest/collection_catalog -Itest/host_stubs -Iinclude `
        test/pet_stat_events/test_main.cpp src/pet/PetStatEvents.cpp `
        src/pet/PetBehaviorRuntimeRules.cpp src/pet/RuntimeValueResolver.cpp -o $outputPath
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    & $outputPath
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    Write-Host '[PASS] Stat event SD records, snapshots, ordered roots, terminal and read failure'
}
finally {
    Pop-Location
}

[CmdletBinding()]
param()

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$repoRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
$outputPath = Join-Path $repoRoot '.pio\pet_behavior_runtime_host.exe'
New-Item -ItemType Directory -Force (Split-Path $outputPath) | Out-Null
Push-Location $repoRoot
try {
    foreach ($statCapacity in @(6, 10)) {
        & g++ -std=c++17 -Wall -Wextra -DENABLE_GUESS_GAME=1 `
            "-DAPP_MAX_PET_STATS=$statCapacity" -Itest/host_stubs -Iinclude `
            test/pet_behavior_runtime/test_main.cpp `
            src/pet/PetBehaviorRuntimeRules.cpp `
            src/pet/RuntimeValueResolver.cpp -o $outputPath
        if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
        & $outputPath
        if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
        Write-Host "[PASS] Action ranges, selection, atomic effects and daily pauses (Stats=$statCapacity)"
    }
}
finally {
    Pop-Location
}

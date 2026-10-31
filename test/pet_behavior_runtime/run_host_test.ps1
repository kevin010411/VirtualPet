[CmdletBinding()]
param()

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$repoRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
$outputPath = Join-Path $repoRoot '.pio\pet_behavior_runtime_host.exe'
New-Item -ItemType Directory -Force (Split-Path $outputPath) | Out-Null
Push-Location $repoRoot
try {
    foreach ($statCapacity in @(6, 10, 12)) {
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
    & g++ -std=c++17 -Wall -Wextra -DENABLE_GUESS_GAME=1 -DAPP_MAX_PET_STATS=12 `
        -DAPP_MAX_ACTION_OUTCOMES=224 -DAPP_MAX_ACTION_EFFECTS=12 -Itest/host_stubs -Iinclude `
        test/pet_behavior_runtime/test_main.cpp src/pet/PetBehaviorRuntimeRules.cpp `
        src/pet/RuntimeValueResolver.cpp -o $outputPath
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    & $outputPath
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    Write-Host '[PASS] Eight Actions x 28 weighted results x 12 streamed effects, exact Version mapping and atomic read failure'
    foreach ($catalogEnabled in @(0, 1)) {
        & g++ -std=c++17 -Wall -Wextra "-DENABLE_COLLECTION_CATALOG=$catalogEnabled" -DENABLE_GUESS_GAME=0 -DAPP_MAX_PET_STATS=6 `
            -Itest/host_stubs -Itest/animation_scene_playback -Iinclude `
            test/pet_behavior_runtime/test_execution.cpp `
            src/pet/PetBehaviorRuntime.cpp src/pet/PetBehaviorRuntimeRules.cpp `
            src/pet/PetStateClassifier.cpp src/pet/Pet.cpp src/pet/RuntimeValueResolver.cpp `
            src/animation/AnimationController.cpp src/animation/BaseAnimationRotation.cpp `
            src/common/FirmwareRandom.cpp -o $outputPath
        if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
        & $outputPath
        if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
        Write-Host "[PASS] Action effects, pauses and catalog count gating; optional primary/following playback (Catalog=$catalogEnabled)"
    }
}
finally {
    Pop-Location
}

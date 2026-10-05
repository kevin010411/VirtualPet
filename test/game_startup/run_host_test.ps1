[CmdletBinding()]
param()

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$repoRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
$outputPath = Join-Path $repoRoot '.pio\game_startup_host.exe'
$sources = @(
    'test/game_startup/test_main.cpp',
    'src/presentation/application/Game.cpp',
    'src/presentation/application/GameStartup.cpp',
    'src/presentation/application/AppFlowController.cpp',
    'src/presentation/application/LayoutRenderer.cpp',
    'src/animation/application/AnimationController.cpp',
    'src/animation/application/BaseAnimationRotation.cpp',
    'src/commands/application/CommandController.cpp',
    'src/commands/application/CommandExecutor.cpp',
    'src/commands/domain/SystemCommandCatalog.cpp',
    'src/commands/domain/StatusSetContract.cpp',
    'src/commands/domain/StatusSetSelection.cpp',
    'src/pet/application/PetActionController.cpp',
    'src/pet/domain/Pet.cpp',
    'src/pet_behavior/application/PetBehaviorRuntime.cpp',
    'src/pet_behavior/domain/PetBehaviorRuntimeRules.cpp',
    'src/pet_behavior/domain/PetStateClassifier.cpp',
    'src/pet_behavior/domain/RuntimeValueResolver.cpp',
    'src/shared/utils/FirmwareRandom.cpp'
)

Push-Location $repoRoot
try {
    & g++ -std=c++17 -O1 -ffunction-sections -fdata-sections '-Wl,--gc-sections' `
        -DENABLE_DEBUG=0 -DENABLE_GUESS_GAME=0 -DENABLE_COMMAND_OUTFIT=0 `
        -DENABLE_COMMAND_PREDICT=0 -DENABLE_APPEARANCE_SELECTION=0 `
        -DENABLE_STARTUP_ANIMATION=0 -DENABLE_FIRST_START_ANIMATION=0 `
        -Itest/game_startup -Itest/host_stubs -Iinclude `
        @sources -o $outputPath
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    & $outputPath
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    Write-Host '[PASS] Game startup, restore, reset, evolution, fatal routing, live Pet transactions and Status'
    exit 0
}
finally {
    Pop-Location
}

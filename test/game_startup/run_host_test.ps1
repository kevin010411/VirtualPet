[CmdletBinding()]
param()

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$repoRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
$outputPath = Join-Path $repoRoot '.pio\game_startup_host.exe'
$sources = @(
    'test/game_startup/test_main.cpp',
    'src/controller/Game.cpp',
    'src/controller/GameStartup.cpp',
    'src/appearance/EvolutionController.cpp',
    'src/controller/AppFlowController.cpp',
    'src/display/LayoutRenderer.cpp',
    'src/animation/AnimationController.cpp',
    'src/animation/BaseAnimationRotation.cpp',
    'src/controller/CommandController.cpp',
    'src/controller/CommandExecutor.cpp',
    'src/controller/SystemCommandCatalog.cpp',
    'src/controller/StatusSetContract.cpp',
    'src/controller/StatusSetSelection.cpp',
    'src/pet/PetSaveController.cpp',
    'src/appearance/AppearanceChangeController.cpp',
    'src/pet/Pet.cpp',
    'src/pet/PetBehaviorRuntime.cpp',
    'src/pet/PetBehaviorRuntimeRules.cpp',
    'src/pet/PetStateClassifier.cpp',
    'src/pet/RuntimeValueResolver.cpp',
    'src/common/FirmwareRandom.cpp'
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
    Write-Host '[PASS] Game startup, restore, reset, evolution, fatal routing, Pet transactions, Status, appearance failure order and save cadence'
    exit 0
}
finally {
    Pop-Location
}

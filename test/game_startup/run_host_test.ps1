[CmdletBinding()]
param([switch]$EnableFirstStartAnimation, [switch]$EnableAppearanceSelection, [switch]$EnableCollectionCatalog, [switch]$DisableDailyPostProcessing, [switch]$SequenceInterruptionOnly)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$repoRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
$outputName = if ($EnableFirstStartAnimation) { 'game_startup_first_start_host.exe' } else { 'game_startup_host.exe' }
if ($EnableAppearanceSelection) { $outputName = $outputName.Replace('_host', '_appearance_host') }
if ($EnableCollectionCatalog) { $outputName = $outputName.Replace('_host', '_collection_host') }
if ($DisableDailyPostProcessing) { $outputName = $outputName.Replace('_host', '_no_daily_post_host') }
$outputPath = Join-Path $repoRoot (Join-Path '.pio' $outputName)
$startupEnabled = [int]$EnableFirstStartAnimation.IsPresent
$appearanceEnabled = [int]$EnableAppearanceSelection.IsPresent
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
    'src/pet/PetSession.cpp',
    'src/appearance/AppearanceChangeController.cpp',
    'src/pet/Pet.cpp',
    'src/pet/PetBehaviorRuntime.cpp',
    'src/pet/PetBehaviorRuntimeRules.cpp',
    'src/pet/PetStateClassifier.cpp',
    'src/pet/RuntimeValueResolver.cpp',
    'src/common/FirmwareRandom.cpp'
)
if ($EnableAppearanceSelection) { $sources += 'src/appearance/AppearanceSelectionController.cpp' }

Push-Location $repoRoot
try {
    & g++ -std=c++17 -O1 -ffunction-sections -fdata-sections '-Wl,--gc-sections' `
        "-DENABLE_DAILY_POST_PROCESSING=$([int](-not $DisableDailyPostProcessing.IsPresent))" `
        -DENABLE_DEBUG=0 -DENABLE_GUESS_GAME=0 "-DENABLE_COLLECTION_CATALOG=$([int]$EnableCollectionCatalog.IsPresent)" "-DENABLE_COMMAND_OUTFIT=$appearanceEnabled" `
        -DENABLE_COMMAND_PREDICT=0 "-DENABLE_APPEARANCE_SELECTION=$appearanceEnabled" `
        "-DENABLE_STARTUP_ANIMATION=$startupEnabled" "-DENABLE_FIRST_START_ANIMATION=$startupEnabled" `
        -Itest/game_startup -Itest/host_stubs -Iinclude `
        @sources -o $outputPath
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    if ($SequenceInterruptionOnly) { & $outputPath --sequence-interruption }
    else { & $outputPath }
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    if ($SequenceInterruptionOnly) {
        Write-Host '[PASS] Command interruption in primary/following segments across day boundaries; RTC catch-up ordering and navigation'
    } else {
        Write-Host '[PASS] PetSession data flow; Game startup, restore, reset, evolution, fatal routing, Pet transactions, Status, appearance failure order and save cadence'
    }
    exit 0
}
finally {
    Pop-Location
}

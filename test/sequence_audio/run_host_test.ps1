[CmdletBinding()]
param([Parameter(Mandatory=$true)][string]$FixtureRoot, [switch]$Silent, [switch]$Idle, [switch]$Disabled, [switch]$Animation)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$repoRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
$testSource = if ($Disabled) { 'test/sequence_audio/test_disabled.cpp' } else { 'test/sequence_audio/test_main.cpp' }
$sources = @($testSource, 'src/audio/SequenceAudio.cpp',
    'src/animation/SequencePlayback.cpp', 'src/animation/AnimationController.cpp',
    'src/animation/BaseAnimationRotation.cpp', 'src/pet/Pet.cpp',
    'src/pet/PetBehaviorRuntime.cpp', 'src/pet/PetBehaviorRuntimeRules.cpp',
    'src/pet/PetStateClassifier.cpp', 'src/pet/RuntimeValueResolver.cpp',
    'src/resources/RuntimeTableBehavior.cpp', 'src/resources/RuntimeTableReader.cpp',
    'src/resources/RuntimeTableFile.cpp', 'src/appearance/RuntimeTableAppearance.cpp',
    'src/controller/SystemCommandCatalog.cpp', 'src/controller/StatusSetContract.cpp',
    'src/common/FirmwareRandom.cpp', 'src/resources/AssetRuntimeContract.cpp', 'src/resources/BundleReader.cpp', 'src/common/CopyResourceName.cpp')
if ($Disabled) {
    $sources = @($testSource, 'src/resources/RuntimeTableBehavior.cpp', 'src/resources/RuntimeTableReader.cpp',
        'src/resources/RuntimeTableFile.cpp', 'src/appearance/RuntimeTableAppearance.cpp',
        'src/pet/RuntimeValueResolver.cpp', 'src/pet/PetBehaviorRuntimeRules.cpp', 'src/controller/SystemCommandCatalog.cpp',
        'src/controller/StatusSetContract.cpp', 'src/resources/AssetRuntimeContract.cpp',
        'src/resources/BundleReader.cpp', 'src/common/CopyResourceName.cpp')
}
Push-Location $repoRoot
try {
    New-Item -ItemType Directory -Force '.pio' | Out-Null
    & g++ -std=c++17 -O1 -ffunction-sections -fdata-sections '-Wl,--gc-sections' `
        "-DENABLE_SEQUENCE_AUDIO=$([int](-not $Disabled.IsPresent))" -DENABLE_DEATH=1 -DENABLE_GUESS_GAME=0 -DENABLE_COLLECTION_CATALOG=0 `
        -DENABLE_STARTUP_ANIMATION=0 -DENABLE_COMMAND_PREDICT=0 -DENABLE_COMMAND_OUTFIT=0 `
        -DENABLE_APPEARANCE_SELECTION=0 -Itest/sequence_audio -Itest/animation_scene_playback -Itest/host_stubs -Iinclude `
        @sources -o '.pio/sequence_audio_host.exe'
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    $arguments = @((Join-Path $FixtureRoot 'runtime.bin'), (Join-Path $FixtureRoot 'audio.data'))
    if ($Disabled) { $arguments = @((Join-Path $FixtureRoot 'no_audio/runtime.bin'), (Join-Path $FixtureRoot 'idle_only/runtime.bin')) }
    if ($Animation) { $arguments += '--animation' }
    elseif ($Silent) { $arguments += '--silent' }
    elseif ($Idle) { $arguments += '--idle' }
    & '.pio/sequence_audio_host.exe' @arguments
    exit $LASTEXITCODE
} finally { Pop-Location }

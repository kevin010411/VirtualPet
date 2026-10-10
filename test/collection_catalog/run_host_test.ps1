[CmdletBinding()]
param([Parameter(Mandatory=$true)][string]$FixtureRoot, [switch]$UseRtcClock)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$repoRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
$sources = @(
 'test/collection_catalog/test_main.cpp', 'src/collection/CollectionCatalog.cpp',
 'src/controller/Game.cpp', 'src/controller/GameStartup.cpp',
 'src/controller/AppFlowController.cpp', 'src/controller/CommandController.cpp',
 'src/controller/CommandExecutor.cpp', 'src/controller/SystemCommandCatalog.cpp',
 'src/controller/StatusSetContract.cpp', 'src/controller/StatusSetSelection.cpp',
 'src/animation/AnimationController.cpp', 'src/animation/BaseAnimationRotation.cpp',
 'src/appearance/EvolutionController.cpp', 'src/appearance/AppearanceChangeController.cpp',
 'src/appearance/SdAppearanceLoader.cpp', 'src/appearance/RuntimeTableAppearance.cpp',
 'src/resources/BundleReader.cpp', 'src/resources/AssetRuntimeContract.cpp',
 'src/resources/RuntimeTableBehavior.cpp', 'src/resources/RuntimeTableReader.cpp',
 'src/resources/RuntimeTableFile.cpp', 'src/resources/RuntimeContractLoader.cpp',
 'src/display/Renderer.cpp', 'src/display/FrameDecoder.cpp', 'src/display/LayoutRenderer.cpp',
 'src/pet/Pet.cpp', 'src/pet/PetStorage.cpp', 'src/pet/PetSession.cpp',
 'src/pet/PetSaveController.cpp', 'src/pet/PetBehaviorRuntime.cpp',
 'src/pet/PetBehaviorRuntimeRules.cpp', 'src/pet/PetStateClassifier.cpp',
 'src/pet/RuntimeValueResolver.cpp', 'src/common/CopyResourceName.cpp',
 'src/common/FirmwareRandom.cpp', 'src/common/Crc32.cpp'
)
Push-Location $repoRoot
try {
 New-Item -ItemType Directory -Force .pio | Out-Null
 & g++ -std=c++17 -O1 -ffunction-sections -fdata-sections '-Wl,--gc-sections' `
  "-DCOLLECTION_USE_RTC=$([int]$UseRtcClock.IsPresent)" `
  -DENABLE_DEBUG=0 -DENABLE_COLLECTION_CATALOG=1 -DENABLE_GUESS_GAME=0 `
  -DENABLE_APPEARANCE_SELECTION=0 -DENABLE_COMMAND_OUTFIT=0 -DENABLE_COMMAND_PREDICT=0 `
  -Itest/collection_catalog -Itest/host_stubs -Iinclude @sources -o .pio/collection_catalog_host.exe
 if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
 & .pio/collection_catalog_host.exe ([IO.Path]::GetFullPath($FixtureRoot))
 exit $LASTEXITCODE
} finally { Pop-Location }

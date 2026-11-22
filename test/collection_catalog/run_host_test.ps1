[CmdletBinding()]
param([Parameter(Mandatory=$true)][string]$FixtureRoot, [switch]$UseRtcClock, [switch]$ActionCounts, [switch]$Grid64, [switch]$Pages64, [switch]$Activation, [switch]$Recovery, [switch]$ActivationEvolution, [Alias('InvalidContracts')][switch]$CapacityLimits, [switch]$GuessGame, [switch]$GuessClassic, [switch]$Shop, [switch]$TransactionsOnly, [switch]$ManualDay, [switch]$ProjectMenus, [switch]$StatEvents, [switch]$CaptureGame, [switch]$Cards)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
if ($GuessClassic -and -not $UseRtcClock) { throw 'GuessClassic requires the production RTC clock.' }
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
 'src/resources/RuntimeTableBehaviorDecoder.cpp', 'src/resources/RuntimeTablePresentationDecoder.cpp',
 'src/resources/RuntimeTableFile.cpp', 'src/resources/RuntimeContractLoader.cpp',
 'src/resources/AssetResources.cpp', 'src/display/Renderer.cpp', 'src/display/FrameDecoder.cpp', 'src/display/LayoutRenderer.cpp',
 'src/pet/Pet.cpp', 'src/pet/PetStorage.cpp', 'src/pet/PetSession.cpp',
 'src/pet/PetSaveController.cpp', 'src/pet/PetBehaviorRuntime.cpp',
 'src/pet/PetBehaviorRuntimeRules.cpp', 'src/pet/PetStateClassifier.cpp',
 'src/pet/RuntimeValueResolver.cpp', 'src/common/CopyResourceName.cpp',
 'src/common/FirmwareRandom.cpp', 'src/common/Crc32.cpp'
)
if ($Shop) { $sources += 'src/shop/ShopSession.cpp', 'src/shop/ShopConfirmation.cpp' }
if ($ProjectMenus) { $sources += 'src/controller/ProjectMenuSession.cpp' }
if ($StatEvents) { $sources += 'src/controller/GameStatEvents.cpp', 'src/pet/PetStatEvents.cpp' }
if ($CaptureGame) {
 $sources += 'src/controller/CaptureOutcome.cpp', 'src/game/CaptureAssets.cpp', 'src/game/CaptureGame.cpp'
}
if ($GuessGame -or $GuessClassic -or $CaptureGame) { $sources += 'src/controller/MinigameController.cpp' }
if ($GuessGame -or $GuessClassic) { $sources += 'src/game/GuessItemGame.cpp' }
$eventProfile = if ($StatEvents) { '-DAPP_MAX_SCREEN_RULES=95' }
$outputPath = if ($CaptureGame) { '.pio/capture_firmware_host.exe' } elseif ($StatEvents) { '.pio/stat_event_firmware_host.exe' } elseif ($Shop) { '.pio/shop_firmware_host.exe' } elseif ($ProjectMenus) { '.pio/project_menu_firmware_host.exe' } else { '.pio/collection_catalog_host.exe' }
Push-Location $repoRoot
try {
 New-Item -ItemType Directory -Force .pio | Out-Null
 & g++ -std=c++17 -O1 -ffunction-sections -fdata-sections '-Wl,--gc-sections' `
  "-DCOLLECTION_USE_RTC=$([int]($UseRtcClock.IsPresent -or $Cards.IsPresent))" `
  "-DENABLE_STAT_EVENTS=$([int]$StatEvents.IsPresent)" `
  "-DENABLE_CAPTURE_GAME=$([int]$CaptureGame.IsPresent)" `
  "-DENABLE_COLLECTION_GAME_EVENTS=$([int]$CaptureGame.IsPresent)" `
  $eventProfile `
  "-DENABLE_PROJECT_MENUS=$([int]$ProjectMenus.IsPresent)" `
  "-DENABLE_SHOP=$([int]$Shop.IsPresent)" -DENABLE_DEBUG=0 -DENABLE_COLLECTION_CATALOG=1 "-DENABLE_GUESS_GAME=$([int]($GuessGame.IsPresent -or $GuessClassic.IsPresent))" -DENABLE_GUESS_GAME_SINGLE_ROUND=1 "-DENABLE_GUESS_GAME_PLAYER_CHOICE_RESULT=$([int](-not $GuessClassic.IsPresent))" `
  "-DENABLE_APPEARANCE_SELECTION=$([int]($Activation.IsPresent -or $ActivationEvolution.IsPresent))" "-DENABLE_COMMAND_OUTFIT=$([int]($Activation.IsPresent -or $ActivationEvolution.IsPresent))" -DENABLE_COMMAND_PREDICT=0 `
  -Itest/collection_catalog -Itest/host_stubs -Iinclude @sources $(if ($Activation -or $ActivationEvolution) { 'src/appearance/AppearanceSelectionController.cpp' }) -o $outputPath
 if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
 & $outputPath ([IO.Path]::GetFullPath($FixtureRoot)) $(if ($Cards) { 'cards' } elseif ($CaptureGame) { 'capture' } elseif ($StatEvents) { 'stat-events' } elseif ($TransactionsOnly) { 'transactions' } elseif ($ManualDay) { 'manual-day' } elseif ($Shop) { 'shop' } elseif ($ProjectMenus) { 'project-menus' } elseif ($GuessClassic) { 'guess-classic' } elseif ($GuessGame) { 'guess' } elseif ($CapacityLimits) { 'capacity' } elseif ($Recovery) { 'recovery' } elseif ($ActivationEvolution) { 'activation-evolution' } elseif ($Activation) { 'activation' } elseif ($ActionCounts) { 'counts' } elseif ($Pages64) { 'pages64' } elseif ($Grid64) { 'grid64' } else { 'single' })
 exit $LASTEXITCODE
} finally { Pop-Location }

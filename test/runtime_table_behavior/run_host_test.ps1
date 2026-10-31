[CmdletBinding()]
param([ValidateSet(0, 1)][int]$DeathEnabled = 0, [string]$WebFixturesRoot = "", [string]$LotteryFixture = "", [string]$IndependentLotteryFixture = "")

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$repoRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
$webFixtures = [IO.Path]::GetFullPath((Join-Path $repoRoot '..\..\web\tests\fixtures'))
if ($WebFixturesRoot) { $webFixtures = [IO.Path]::GetFullPath($WebFixturesRoot) }
$validFixture = Join-Path $webFixtures 'asset_data_v2_layout\coherent-bundle\runtime.bin'
$legacyFixture = Join-Path $webFixtures 'runtime_table_v6\minimal\runtime.bin'
$startupFixture = Join-Path $webFixtures 'runtime_table_v8\outfit_selection_release\runtime.bin'
$sequenceFixture = Join-Path $webFixtures 'runtime_table_v8\behavior_sequence\runtime.bin'
$optionalFixture = Join-Path $webFixtures 'runtime_table_v8\behavior_optional_animation\runtime.bin'
$appliedFixtureRoot = Join-Path $webFixtures 'applied_screen_layout_v8'
$appliedFixtures = @('valid', 'bad_geometry', 'bad_button_source', 'unsupported_stat',
    'unsupported_rules', 'duplicate_animation', 'over_capacity', 'missing_screen_blocks') |
    ForEach-Object { Join-Path $appliedFixtureRoot "$_\runtime.bin" }
$outputPath = Join-Path $repoRoot '.pio\runtime_table_behavior_host.exe'
if (-not $LotteryFixture -and -not $IndependentLotteryFixture) {
    foreach ($fixturePath in @($validFixture, $legacyFixture, $startupFixture, $sequenceFixture, $optionalFixture) + $appliedFixtures) {
        if (-not (Test-Path -LiteralPath $fixturePath -PathType Leaf)) {
            throw "Web exporter fixture is missing: $fixturePath"
        }
    }
}
$sources = @(
    'test/runtime_table_behavior/test_main.cpp',
    'src/appearance/SdAppearanceLoader.cpp',
    'src/resources/RuntimeTableBehavior.cpp',
    'src/resources/RuntimeTableBehaviorDecoder.cpp',
    'src/resources/RuntimeTablePresentationDecoder.cpp',
    'src/appearance/RuntimeTableAppearance.cpp',
    'src/resources/RuntimeTableReader.cpp',
    'src/resources/RuntimeTableFile.cpp',
    'src/pet/PetBehaviorRuntimeRules.cpp',
    'src/pet/RuntimeValueResolver.cpp',
    'src/controller/StatusSetContract.cpp',
    'src/controller/SystemCommandCatalog.cpp',
    'src/common/CopyResourceName.cpp'
)
Push-Location $repoRoot
try {
    if ($IndependentLotteryFixture) {
        & g++ -std=c++17 -DRUNTIME_TABLE_INDEPENDENT_LOTTERY=1 -DENABLE_GUESS_GAME=1 -DENABLE_STARTUP_ANIMATION=1 `
            -DAPP_MAX_PET_STATS=12 -DAPP_MAX_ACTIONS=16 -DAPP_MAX_ACTION_OUTCOMES=248 -DAPP_MAX_ACTION_EFFECTS=12 `
            -DAPP_MAX_SCREEN_RULES=64 -Itest/host_stubs -Iinclude @sources -o $outputPath
        if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
        & $outputPath $IndependentLotteryFixture
        exit $LASTEXITCODE
    }
    if ($LotteryFixture) {
        & g++ -std=c++17 -DRUNTIME_TABLE_LOTTERY=1 -DENABLE_GUESS_GAME=1 -DENABLE_STARTUP_ANIMATION=1 `
            -DAPP_MAX_PET_STATS=12 -DAPP_MAX_ACTION_OUTCOMES=224 -DAPP_MAX_ACTION_EFFECTS=12 `
            -DAPP_MAX_SCREEN_RULES=64 -Itest/host_stubs -Iinclude @sources -o $outputPath
        if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
        & $outputPath $LotteryFixture
        if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
        Write-Host '[PASS] Actual Web export: 224 fixed-Version outcomes / 2688 streamed effects, all result mappings and values'
        exit 0
    }
    & g++ -std=c++17 -DRUNTIME_TABLE_V8=1 -DENABLE_GUESS_GAME=1 `
        "-DENABLE_DEATH=$DeathEnabled" `
        -DAPP_MAX_PET_STATS=10 -Itest/host_stubs -Iinclude @sources -o $outputPath
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    & $outputPath $validFixture $legacyFixture $startupFixture @appliedFixtures $sequenceFixture $optionalFixture
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    Write-Host '[PASS] Runtime Table v27 optional action animations, action ranges, applied screen, initial contract, malformed records, and legacy rejection'
    exit 0
}
finally {
    Pop-Location
}

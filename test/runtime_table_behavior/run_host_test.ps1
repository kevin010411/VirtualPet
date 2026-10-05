[CmdletBinding()]
param()

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$repoRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
$webFixtures = [IO.Path]::GetFullPath((Join-Path $repoRoot '..\..\web\tests\fixtures'))
$validFixture = Join-Path $webFixtures 'asset_data_v2_layout\coherent-bundle\runtime.bin'
$legacyFixture = Join-Path $webFixtures 'runtime_table_v6\minimal\runtime.bin'
$startupFixture = Join-Path $webFixtures 'runtime_table_v8\outfit_selection_release\runtime.bin'
$appliedFixtureRoot = Join-Path $webFixtures 'applied_screen_layout_v8'
$appliedFixtures = @('valid', 'bad_geometry', 'bad_button_source', 'unsupported_stat',
    'unsupported_rules', 'duplicate_animation', 'over_capacity', 'missing_screen_blocks') |
    ForEach-Object { Join-Path $appliedFixtureRoot "$_\runtime.bin" }
$outputPath = Join-Path $repoRoot '.pio\runtime_table_behavior_host.exe'
foreach ($fixturePath in @($validFixture, $legacyFixture, $startupFixture) + $appliedFixtures) {
    if (-not (Test-Path -LiteralPath $fixturePath -PathType Leaf)) {
        throw "Web exporter fixture is missing: $fixturePath"
    }
}
$sources = @(
    'test/runtime_table_behavior/test_main.cpp',
    'src/appearance/SdAppearanceLoader.cpp',
    'src/resources/RuntimeTableBehavior.cpp',
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
    & g++ -std=c++17 -DRUNTIME_TABLE_V8=1 -DENABLE_GUESS_GAME=1 `
        -DAPP_MAX_PET_STATS=10 -Itest/host_stubs -Iinclude @sources -o $outputPath
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    & $outputPath $validFixture $legacyFixture $startupFixture @appliedFixtures
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    Write-Host '[PASS] Runtime Table v9 action ranges, applied screen, initial contract, malformed records, and legacy rejection'
    exit 0
}
finally {
    Pop-Location
}

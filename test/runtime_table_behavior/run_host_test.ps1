[CmdletBinding()]
param()

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$repoRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
$webFixtures = [IO.Path]::GetFullPath((Join-Path $repoRoot '..\..\web\tests\fixtures'))
$validFixture = Join-Path $webFixtures 'asset_data_v2_layout\coherent-bundle\runtime.bin'
$legacyFixture = Join-Path $webFixtures 'runtime_table_v6\minimal\runtime.bin'
$startupFixture = Join-Path $webFixtures 'runtime_table_v7\outfit_selection_release\runtime.bin'
$outputPath = Join-Path $repoRoot '.pio\runtime_table_behavior_host.exe'
foreach ($fixturePath in @($validFixture, $legacyFixture, $startupFixture)) {
    if (-not (Test-Path -LiteralPath $fixturePath -PathType Leaf)) {
        throw "Web exporter fixture is missing: $fixturePath"
    }
}
$sources = @(
    'test/runtime_table_behavior/test_main.cpp',
    'src/pet_behavior/domain/RuntimeTableBehavior.cpp',
    'src/pet_behavior/domain/PetBehaviorRuntimeRules.cpp',
    'src/pet_behavior/domain/RuntimeValueResolver.cpp',
    'src/commands/domain/StatusSetContract.cpp',
    'src/commands/domain/SystemCommandCatalog.cpp'
)
Push-Location $repoRoot
try {
    & g++ -std=c++17 -DRUNTIME_TABLE_V7=1 -DENABLE_GUESS_GAME=1 `
        -DAPP_MAX_PET_STATS=10 -Itest/host_stubs -Iinclude @sources -o $outputPath
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    & $outputPath $validFixture $legacyFixture $startupFixture
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    Write-Host '[PASS] Runtime Table v7 layout, initial contract, and v6 rejection'
    exit 0
}
finally {
    Pop-Location
}

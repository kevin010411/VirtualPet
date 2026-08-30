[CmdletBinding()]
param()

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$repoRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
$fixtureRoot = [IO.Path]::GetFullPath((Join-Path $repoRoot '..\..\web\tests\fixtures\runtime_table_v1'))
$outputPath = Join-Path $repoRoot '.pio\runtime_table_behavior_host.exe'
$fullFeatureOutputPath = Join-Path $repoRoot '.pio\runtime_table_behavior_full_host.exe'
$fixturePaths = @(
    (Join-Path $fixtureRoot 'behavior_full\runtime.bin'),
    (Join-Path $fixtureRoot 'stat_bounds\runtime.bin'),
    (Join-Path $fixtureRoot 'bad_crc\runtime.bin')
)
foreach ($fixturePath in $fixturePaths) {
    if (-not (Test-Path -LiteralPath $fixturePath -PathType Leaf)) {
        throw "Web exporter fixture is missing: $fixturePath"
    }
}

$sources = @(
    'test/runtime_table_behavior/test_main.cpp',
    'src/pet_behavior/domain/RuntimeTableBehavior.cpp',
    'src/pet_behavior/domain/PetBehaviorRuntimeRules.cpp',
    'src/pet_behavior/domain/PetBehaviorActionConditionRules.cpp',
    'src/commands/domain/StatusSetContract.cpp',
    'src/commands/domain/SystemCommandCatalog.cpp'
)

Push-Location $repoRoot
try {
    & g++ -std=c++17 -DENABLE_GUESS_GAME=1 -DAPP_MAX_PET_STATS=10 `
        -Itest/host_stubs -Iinclude @sources -o $outputPath
    if ($LASTEXITCODE -ne 0) {
        exit $LASTEXITCODE
    }
    & $outputPath @fixturePaths
    if ($LASTEXITCODE -ne 0) {
        exit $LASTEXITCODE
    }

    $fullFeatureFixture = Join-Path $fixtureRoot 'appearance_flow_full\runtime.bin'
    & g++ -std=c++17 -DRUNTIME_TABLE_FULL_FEATURE=1 `
        -DENABLE_GUESS_GAME=1 -DENABLE_STARTUP_ANIMATION=1 `
        -DENABLE_COMMAND_OUTFIT=1 -DENABLE_COMMAND_SPECIES=1 `
        -DENABLE_DYNAMIC_ACTION_LAYOUT=1 -DENABLE_COMMAND_PREDICT=1 `
        -DENABLE_FIRST_START_ANIMATION=1 -DENABLE_FIRST_LAUNCH_SELECTION=1 `
        -DENABLE_OUTFIT_CHOOSE_ANIMATION=1 -DENABLE_APPEARANCE_SELECTION=1 `
        -DENABLE_GUESS_GAME_SINGLE_ROUND=0 -DENABLE_GUESS_GAME_PLAYER_CHOICE_RESULT=1 `
        -DENABLE_SEQUENTIAL_STATUS_SET_SELECTION=1 -DAPP_MAX_PET_STATS=10 `
        -Itest/host_stubs -Iinclude @sources -o $fullFeatureOutputPath
    if ($LASTEXITCODE -ne 0) {
        exit $LASTEXITCODE
    }
    & $fullFeatureOutputPath $fullFeatureFixture
    exit $LASTEXITCODE
}
finally {
    Pop-Location
}

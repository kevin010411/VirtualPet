[CmdletBinding()]
param()

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$repoRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
$fixtureRoot = [IO.Path]::GetFullPath((Join-Path $repoRoot '..\..\web\tests\fixtures\runtime_table_v1'))
$outputPath = Join-Path $repoRoot '.pio\runtime_table_behavior_host.exe'
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
    exit $LASTEXITCODE
}
finally {
    Pop-Location
}

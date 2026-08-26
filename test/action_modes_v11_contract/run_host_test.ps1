[CmdletBinding()]
param()

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$repoRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
$fixturePath = [IO.Path]::GetFullPath((Join-Path $repoRoot '..\..\web\tests\fixtures\action_modes_v11\runtime_contract.txt'))
$outputPath = Join-Path $repoRoot '.pio\action_modes_v11_contract_host.exe'

if (-not (Test-Path -LiteralPath $fixturePath -PathType Leaf)) {
    throw "Web exporter fixture is missing: $fixturePath"
}

$sources = @(
    'test/action_modes_v11_contract/test_main.cpp',
    'src/pet_behavior/domain/PetBehaviorContract.cpp',
    'src/pet_behavior/domain/PetBehaviorRuntimeRules.cpp',
    'src/pet_behavior/domain/PetBehaviorActionConditionRules.cpp',
    'src/shared/sd/SdTextRecordReader.cpp',
    'src/commands/domain/SystemCommandCatalog.cpp'
)

Push-Location $repoRoot
try {
    & g++ -std=c++17 -DENABLE_GUESS_GAME=1 -DAPP_MAX_PET_STATS=6 `
        -Itest/host_stubs -Iinclude @sources -o $outputPath
    if ($LASTEXITCODE -ne 0) {
        exit $LASTEXITCODE
    }
    & $outputPath $fixturePath
    exit $LASTEXITCODE
}
finally {
    Pop-Location
}

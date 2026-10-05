[CmdletBinding()]
param()

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$repoRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
$fixtureRoot = [IO.Path]::GetFullPath((Join-Path $repoRoot '..\..\web\tests\fixtures\asset_data_v2_layout\coherent-bundle'))
$validFixture = Join-Path $fixtureRoot 'runtime.bin'
$invalidFixture = Join-Path $fixtureRoot 'assets\species_1.data'
$outputPath = Join-Path $repoRoot '.pio\animation_scene_playback_host.exe'
$fixturePaths = @($validFixture, $invalidFixture)
foreach ($fixturePath in $fixturePaths) {
    if (-not (Test-Path -LiteralPath $fixturePath -PathType Leaf)) {
        throw "Web exporter fixture is missing: $fixturePath"
    }
}
$sources = @(
    'test/animation_scene_playback/test_main.cpp',
    'src/animation/application/AnimationController.cpp',
    'src/animation/application/BaseAnimationRotation.cpp',
    'src/commands/application/CommandController.cpp',
    'src/commands/domain/SystemCommandCatalog.cpp',
    'src/commands/domain/StatusSetContract.cpp',
    'src/pet_behavior/domain/PetBehaviorRuntimeRules.cpp',
    'src/pet_behavior/domain/RuntimeValueResolver.cpp',
    'src/pet_behavior/domain/RuntimeTableBehavior.cpp',
    'src/appearance/domain/RuntimeTableAppearance.cpp',
    'src/shared/runtime_table/RuntimeTableReader.cpp',
    'src/shared/runtime_table/RuntimeTableFile.cpp',
    'src/presentation/application/LayoutRenderer.cpp'
    'src/shared/integrity/Crc32.cpp'
    'src/shared/utils/FirmwareRandom.cpp'
)

Push-Location $repoRoot
try {
    & g++ -std=c++17 -DENABLE_DYNAMIC_ACTION_LAYOUT=1 -DAPP_MAX_PET_STATS=10 `
        -DENABLE_COMMAND_PREDICT=0 -DENABLE_GUESS_GAME=0 `
        -DENABLE_COMMAND_OUTFIT=0 `
        -Itest/host_stubs -Itest/animation_scene_playback -Iinclude @sources -o $outputPath
    if ($LASTEXITCODE -ne 0) {
        exit $LASTEXITCODE
    }
    & $outputPath $validFixture $invalidFixture
    if ($LASTEXITCODE -ne 0) {
        exit $LASTEXITCODE
    }
    Write-Host '[PASS] Animation Scene playback: atomic first frame, completion, queue, interruption, Idle fallback'
    exit 0
}
finally {
    Pop-Location
}

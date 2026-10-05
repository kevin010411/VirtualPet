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
    'src/animation/AnimationController.cpp',
    'src/animation/BaseAnimationRotation.cpp',
    'src/controller/CommandController.cpp',
    'src/controller/SystemCommandCatalog.cpp',
    'src/controller/StatusSetContract.cpp',
    'src/pet/PetBehaviorRuntimeRules.cpp',
    'src/pet/RuntimeValueResolver.cpp',
    'src/resources/RuntimeTableBehavior.cpp',
    'src/appearance/RuntimeTableAppearance.cpp',
    'src/resources/RuntimeTableReader.cpp',
    'src/resources/RuntimeTableFile.cpp',
    'src/display/LayoutRenderer.cpp'
    'src/common/Crc32.cpp'
    'src/common/FirmwareRandom.cpp'
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

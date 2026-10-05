[CmdletBinding()]
param([switch]$Numeric)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$repoRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
$fixtures = [IO.Path]::GetFullPath((Join-Path $repoRoot '..\..\web\tests\fixtures\custom_layout_v8'))
if ($Numeric) {
    $fixtures = [IO.Path]::GetFullPath((Join-Path $repoRoot '..\..\web\tests\fixtures\stat_layout_v9'))
    if (-not (Test-Path -LiteralPath (Join-Path $fixtures 'frames.tsv') -PathType Leaf)) {
        throw 'Run Web scripts/generate_stat_layout_fixtures.py at ticket 05 first.'
    }
}
$outputPath = Join-Path $repoRoot '.pio\custom_layout_export_host.exe'
foreach ($case in $(if ($Numeric) { @() } else { @('moved', 'enlarged', 'shrunk-duplicates', 'animation-only', 'buttons-only', 'empty') })) {
    if (-not (Test-Path -LiteralPath (Join-Path $fixtures "$case\frames.tsv") -PathType Leaf)) {
        throw 'Run Web scripts/generate_custom_layout_fixtures.py at ticket 05 first.'
    }
}
$sources = @(
    'test/custom_layout_export/test_main.cpp',
    'src/shared/assets/BundleReader.cpp', 'src/shared/assets/AssetRuntimeContract.cpp',
    'src/shared/utils/CopyResourceName.cpp',
    'src/presentation/adapters/rendering/Renderer.cpp',
    'src/presentation/adapters/rendering/FrameDecoder.cpp',
    'src/presentation/application/LayoutRenderer.cpp',
    'src/commands/application/CommandController.cpp',
    'src/commands/domain/SystemCommandCatalog.cpp', 'src/commands/domain/StatusSetContract.cpp',
    'src/pet_behavior/domain/RuntimeTableBehavior.cpp',
    'src/appearance/domain/RuntimeTableAppearance.cpp',
    'src/shared/runtime_table/RuntimeTableReader.cpp',
    'src/shared/runtime_table/RuntimeTableFile.cpp',
    'src/pet_behavior/domain/PetBehaviorRuntimeRules.cpp', 'src/pet_behavior/domain/RuntimeValueResolver.cpp'
)
Push-Location $repoRoot
try {
    New-Item -ItemType Directory -Path (Split-Path -Parent $outputPath) -Force | Out-Null
    & g++ -std=c++17 -DENABLE_DEBUG=0 -DAPP_MAX_PET_STATS=10 -DENABLE_COMMAND_OUTFIT=1 `
        -DENABLE_COMMAND_PREDICT=1 -DENABLE_GUESS_GAME=1 `
        -DENABLE_STARTUP_ANIMATION=1 -DENABLE_FIRST_START_ANIMATION=1 -DENABLE_OUTFIT_CHOOSE_ANIMATION=1 `
        -Itest/custom_layout_export -Itest/host_stubs -Iinclude @sources -o $outputPath
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    if ($Numeric) { & $outputPath $fixtures --numeric }
    else { & $outputPath $fixtures }
    exit $LASTEXITCODE
}
finally { Pop-Location }

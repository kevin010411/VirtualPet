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
    'src/resources/BundleReader.cpp', 'src/resources/AssetRuntimeContract.cpp',
    'src/common/CopyResourceName.cpp',
    'src/display/Renderer.cpp',
    'src/display/FrameDecoder.cpp',
    'src/display/LayoutRenderer.cpp',
    'src/controller/CommandController.cpp',
    'src/controller/SystemCommandCatalog.cpp', 'src/controller/StatusSetContract.cpp',
    'src/resources/RuntimeTableBehavior.cpp',
    'src/appearance/RuntimeTableAppearance.cpp',
    'src/resources/RuntimeTableReader.cpp',
    'src/resources/RuntimeTableFile.cpp',
    'src/pet/PetBehaviorRuntimeRules.cpp', 'src/pet/RuntimeValueResolver.cpp'
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

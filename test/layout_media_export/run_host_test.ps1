[CmdletBinding()]
param()

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$repoRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
$fixtureRoot = [IO.Path]::GetFullPath((Join-Path $repoRoot '..\..\web\tests\fixtures\layout_media_v8'))
$outputPath = Join-Path $repoRoot '.pio\layout_media_export_host.exe'
foreach ($case in @('contain-animation_area', 'contain-full_device', 'crop-animation_area', 'crop-full_device')) {
    if (-not (Test-Path -LiteralPath (Join-Path $fixtureRoot "$case\frames.tsv") -PathType Leaf)) {
        throw 'Run Web scripts/generate_layout_media_fixtures.py at ticket 05 first.'
    }
}
Push-Location $repoRoot
try {
    New-Item -ItemType Directory -Path (Split-Path -Parent $outputPath) -Force | Out-Null
    & g++ -std=c++17 -DENABLE_DEBUG=1 -Itest/layout_media_export -Itest/bundle_reader_on_access -Itest/host_stubs -Iinclude `
        test/layout_media_export/test_main.cpp `
        src/shared/assets/BundleReader.cpp `
        src/shared/utils/CopyResourceName.cpp `
        src/presentation/adapters/rendering/FrameDecoder.cpp `
        -o $outputPath
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    & $outputPath $fixtureRoot
    exit $LASTEXITCODE
}
finally { Pop-Location }

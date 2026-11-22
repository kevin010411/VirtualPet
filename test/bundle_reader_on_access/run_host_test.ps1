[CmdletBinding()]
param([switch]$Gd32Dma, [string]$WebFixture)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$repoRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
$outputPath = Join-Path $repoRoot '.pio\bundle_reader_on_access_host.exe'
$defines = @('-DENABLE_DEBUG=1')
$arguments = @()
if ($WebFixture) {
    $arguments += (Resolve-Path -LiteralPath $WebFixture).Path
}
if ($Gd32Dma) {
    $outputPath = Join-Path $repoRoot '.pio\bundle_reader_gd32_dma_host.exe'
    $defines = @('-DENABLE_DEBUG=0', '-DSTM32F103xB')
}

Push-Location $repoRoot
try {
    New-Item -ItemType Directory -Force (Split-Path $outputPath) | Out-Null
    & g++ -std=c++17 @defines -Itest/bundle_reader_on_access -Itest/host_stubs -Iinclude `
        test/bundle_reader_on_access/test_main.cpp `
        src/resources/AssetResources.cpp `
        src/resources/BundleReader.cpp `
        src/common/CopyResourceName.cpp `
        src/display/FrameDecoder.cpp `
        -o $outputPath
    if ($LASTEXITCODE -ne 0) {
        exit $LASTEXITCODE
    }
    & $outputPath @arguments
    exit $LASTEXITCODE
}
finally {
    Pop-Location
}

[CmdletBinding()]
param()

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$repoRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
$outputPath = Join-Path $repoRoot '.pio\bundle_reader_on_access_host.exe'

Push-Location $repoRoot
try {
    & g++ -std=c++17 -DENABLE_DEBUG=1 -Itest/bundle_reader_on_access -Itest/host_stubs -Iinclude `
        test/bundle_reader_on_access/test_main.cpp `
        src/resources/BundleReader.cpp `
        src/common/CopyResourceName.cpp `
        src/display/FrameDecoder.cpp `
        -o $outputPath
    if ($LASTEXITCODE -ne 0) {
        exit $LASTEXITCODE
    }
    & $outputPath
    exit $LASTEXITCODE
}
finally {
    Pop-Location
}

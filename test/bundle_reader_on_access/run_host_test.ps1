[CmdletBinding()]
param()

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$repoRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
$outputPath = Join-Path $repoRoot '.pio\bundle_reader_on_access_host.exe'

Push-Location $repoRoot
try {
    & g++ -std=c++17 -Itest/host_stubs -Iinclude `
        test/bundle_reader_on_access/test_main.cpp `
        src/shared/assets/BundleReader.cpp `
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

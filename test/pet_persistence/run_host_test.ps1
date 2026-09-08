[CmdletBinding()]
param()

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$repoRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
$outputPath = Join-Path $repoRoot '.pio\pet_persistence_host.exe'

Push-Location $repoRoot
try {
    & g++ -std=c++17 -Itest/host_stubs -Iinclude `
        test/pet_persistence/test_main.cpp `
        src/pet/domain/Pet.cpp `
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

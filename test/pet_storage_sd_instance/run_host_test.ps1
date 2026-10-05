[CmdletBinding()]
param()

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$repoRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
$outputPath = Join-Path $repoRoot '.pio\pet_storage_sd_instance_host.exe'

Push-Location $repoRoot
try {
    & g++ -std=c++17 -DENABLE_DEBUG=1 -Itest/pet_storage_sd_instance -Iinclude `
        test/pet_storage_sd_instance/test_main.cpp `
        src/pet/Pet.cpp `
        src/pet/PetStorage.cpp `
        src/common/Crc32.cpp `
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

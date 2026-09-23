[CmdletBinding()]
param()

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$repoRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
$outputPath = Join-Path $repoRoot '.pio\runtime_contract_loader_host.exe'

Push-Location $repoRoot
try {
    & g++ -std=c++17 -Itest/host_stubs -Iinclude `
        test/runtime_contract_loader/test_main.cpp `
        src/pet_behavior/domain/RuntimeContractLoader.cpp `
        -o $outputPath
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    & $outputPath
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    Write-Host '[PASS] Runtime contract manifest reuse and error routing'
    exit 0
}
finally {
    Pop-Location
}

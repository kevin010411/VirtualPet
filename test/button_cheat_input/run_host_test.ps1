[CmdletBinding()]
param()

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$repoRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
$outputPath = Join-Path $repoRoot '.pio\button_cheat_input_host.exe'
Push-Location $repoRoot
try {
    & g++ -std=c++17 -Itest/button_cheat_input -Iinclude `
        test/button_cheat_input/test_main.cpp `
        src/platform/hardware/ButtonInput.cpp `
        -o $outputPath
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    & $outputPath
    exit $LASTEXITCODE
}
finally { Pop-Location }

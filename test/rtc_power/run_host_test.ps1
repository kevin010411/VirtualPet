Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$repoRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
Push-Location $repoRoot
try {
    & g++ -std=c++17 -Iinclude test/rtc_power/test_main.cpp -o .pio/rtc_power_host.exe
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    & .pio/rtc_power_host.exe
    exit $LASTEXITCODE
}
finally { Pop-Location }

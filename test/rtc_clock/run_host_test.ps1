Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$repoRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
Push-Location $repoRoot
try {
    & g++ -std=c++17 -Itest/rtc_clock -Iinclude test/rtc_clock/test_main.cpp src/platform/RtcClock.cpp -o .pio/rtc_clock_host.exe
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    & .pio/rtc_clock_host.exe
    exit $LASTEXITCODE
}
finally { Pop-Location }

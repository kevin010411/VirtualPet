[CmdletBinding()]
param()

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$repoRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
$outputPath = Join-Path $repoRoot '.pio\renderer_startup_error_host.exe'

Push-Location $repoRoot
try {
    & g++ -std=c++17 -DENABLE_DEBUG=1 `
        -Itest/renderer_startup_error -Itest/host_stubs -Iinclude `
        test/renderer_startup_error/test_main.cpp `
        src/shared/assets/BundleReader.cpp `
        src/presentation/adapters/rendering/FrameDecoder.cpp `
        src/presentation/adapters/rendering/Renderer.cpp `
        src/presentation/adapters/rendering/TftDebugDisplay.cpp `
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

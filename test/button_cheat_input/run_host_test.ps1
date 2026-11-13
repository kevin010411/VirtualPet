[CmdletBinding()]
param([switch]$ProjectMenus)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$repoRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
$outputName = if ($ProjectMenus) { 'button_project_menu_host.exe' } else { 'button_cheat_input_host.exe' }
$outputPath = Join-Path $repoRoot ".pio\$outputName"
Push-Location $repoRoot
try {
    & g++ -std=c++17 -Itest/button_cheat_input -Iinclude `
        "-DENABLE_PROJECT_MENUS=$([int]$ProjectMenus.IsPresent)" `
        test/button_cheat_input/test_main.cpp `
        src/platform/ButtonInput.cpp `
        -o $outputPath
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    & $outputPath
    exit $LASTEXITCODE
}
finally { Pop-Location }

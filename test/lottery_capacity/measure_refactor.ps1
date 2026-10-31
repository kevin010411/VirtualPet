[CmdletBinding()]
param(
    [Parameter(Mandatory)][ValidatePattern('^[a-z0-9-]+$')][string]$Stage,
    [string]$LibraryRoot = "",
    [switch]$IndependentOnly,
    [string]$Pio = "$env:USERPROFILE/.platformio/penv/Scripts/pio.exe"
)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$repoRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$outputRoot = Join-Path $repoRoot '.pio/memory-refactor'
$stageRoot = Join-Path $outputRoot $Stage
New-Item -ItemType Directory -Force $stageRoot | Out-Null
if (-not $LibraryRoot) { $LibraryRoot = Join-Path $repoRoot '.pio/libdeps/project_33' }
$libraryPath = [IO.Path]::GetFullPath($LibraryRoot).Replace('\', '/')
$root = $repoRoot.Replace('\', '/')
$output = $stageRoot.Replace('\', '/')
$configPath = Join-Path $stageRoot 'platformio.ini'
$buildRoot = Join-Path $stageRoot 'build'
@"
[platformio]
src_dir = $root/src
include_dir = $root/include
build_dir = $($buildRoot.Replace('\', '/'))
[env]
platform = ststm32
board = genericSTM32F103C8
framework = arduino
lib_extra_dirs = $libraryPath
build_flags = -Os -flto -ffunction-sections -fdata-sections -fno-exceptions -DENABLE_DEBUG=0 -DENABLE_DEBUG_UART=0 -DAPP_MAX_PET_STATS=12 -DAPP_MAX_ACTIONS=8 -DAPP_MAX_ACTION_OUTCOMES=224 -DAPP_MAX_ACTION_EFFECTS=12 -DENABLE_STARTUP_ANIMATION=1
build_unflags = -Og -O0 -O1 -O2 -O3
[env:maximum]
build_flags = `${env.build_flags} -DAPP_MAX_SCREEN_RULES=1 -Wl,-Map,$output/maximum.map
[env:independent]
build_flags = -Os -flto -ffunction-sections -fdata-sections -fno-exceptions -DENABLE_DEBUG=0 -DENABLE_DEBUG_UART=0 -DAPP_MAX_PET_STATS=12 -DAPP_MAX_ACTIONS=16 -DAPP_MAX_ACTION_OUTCOMES=248 -DAPP_MAX_ACTION_EFFECTS=12 -DENABLE_STARTUP_ANIMATION=1 -DAPP_MAX_SCREEN_RULES=1 -Wl,-Map,$output/independent.map
[env:allfeatures]
build_flags = `${env.build_flags} -DAPP_MAX_SCREEN_RULES=1 -DENABLE_SEQUENCE_AUDIO=1 -DENABLE_GUESS_GAME=1 -DENABLE_COMMAND_OUTFIT=1 -DENABLE_APPEARANCE_SELECTION=1 -DENABLE_COLLECTION_CATALOG=1 -DENABLE_INSTRUCTION_RESPONSE=1 -DENABLE_COMMAND_PREDICT=1 -DENABLE_DEATH=1 -DENABLE_FIRST_START_ANIMATION=1 -DENABLE_OUTFIT_CHOOSE_ANIMATION=1 -DENABLE_GUESS_GAME_SINGLE_ROUND=1 -DENABLE_GUESS_GAME_PLAYER_CHOICE_RESULT=1 -Wl,-Map,$output/allfeatures.map
[env:numeric]
build_flags = `${env.build_flags} -DAPP_MAX_SCREEN_RULES=200 -DENABLE_GUESS_GAME=1 -Wl,-Map,$output/numeric.map
"@ | Set-Content -LiteralPath $configPath -Encoding utf8
$toolchain = "$env:USERPROFILE/.platformio/packages/toolchain-gccarmnoneeabi/bin"
$probe = Join-Path $stageRoot 'types.cpp'
$resourceProbe = if (Test-Path (Join-Path $repoRoot 'include/resources/AssetResources.h')) {
    'char size_asset_resources[sizeof(AssetResources)];'
} else { '' }
@"
#include "$root/src/display/Renderer.cpp"
#include "pet/PetBehaviorTypes.h"
#include "resources/detail/RuntimeTableReader.h"
char size_renderer_heap[sizeof(Renderer::AnimationState)];
$resourceProbe
char size_display_buffers[FrameDecoder::kDataReadBufferBytes + FrameDecoder::kLineBufferPixels * sizeof(uint16_t)];
char size_outcome[sizeof(PetBehaviorActionOutcomeConfig)];
char size_rule[sizeof(ScreenRuleConfig)];
char size_runtime_table[sizeof(RuntimeTableInternal::RuntimeTable)];
"@ | Set-Content -LiteralPath $probe -Encoding utf8
$measurements = @()
Push-Location $repoRoot
try {
    # This object is a size probe only; dependency stubs are never linked into firmware.
    & "$toolchain/arm-none-eabi-g++.exe" -mcpu=cortex-m3 -mthumb -std=c++17 -Os -fno-access-control `
        -DENABLE_DEBUG=0 -Itest/custom_layout_export -Itest/host_stubs -Iinclude -c $probe -o (Join-Path $stageRoot 'types.o')
    if ($LASTEXITCODE -ne 0) { throw 'ARM type-size probe failed.' }
    $typeSizes = @{}
    & "$toolchain/arm-none-eabi-nm.exe" -S --radix=d (Join-Path $stageRoot 'types.o') | ForEach-Object {
        if ($_ -match '^\d+\s+(\d+)\s+B\s+(size_\w+)$') { $typeSizes[$Matches[2]] = [int]$Matches[1] }
    }
    if (-not $typeSizes.ContainsKey('size_renderer_heap')) { throw 'ARM type sizes were not reported.' }
    $typeSizes | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $stageRoot 'types.json') -Encoding utf8
    if (-not $typeSizes.ContainsKey('size_asset_resources')) { $typeSizes['size_asset_resources'] = 0 }
    $profiles = if ($IndependentOnly) { @('independent') } else { @('maximum', 'allfeatures', 'numeric') }
    foreach ($profile in $profiles) {
        $logPath = Join-Path $stageRoot "$profile.log"
        & $Pio run -c $configPath -e $profile *> $logPath
        $buildExit = $LASTEXITCODE
        $mapPath = Join-Path $buildRoot "$profile/firmware.map"
        if (-not (Test-Path -LiteralPath $mapPath)) { throw "Missing linker map; inspect $logPath" }
        Copy-Item -LiteralPath $mapPath -Destination (Join-Path $stageRoot "$profile.map")
        $sections = @{}
        foreach ($line in Get-Content -LiteralPath $mapPath) {
            if ($line -match '^(\.[\w.]+)\s+0x([0-9a-f]+)\s+0x([0-9a-f]+)(?: load address 0x([0-9a-f]+))?\s*$') {
                $sections[$Matches[1]] = [pscustomobject]@{
                    Address = [Convert]::ToUInt32($Matches[2], 16)
                    Bytes = [Convert]::ToUInt32($Matches[3], 16)
                    LoadAddress = $(if ($Matches[4]) { [Convert]::ToUInt32($Matches[4], 16) } else { 0 })
                }
            }
        }
        $flash = 0
        foreach ($section in $sections.Values) {
            if (($section.Address -ge 0x08000000 -and $section.Address -lt 0x10000000) -or
                ($section.LoadAddress -ge 0x08000000 -and $section.LoadAddress -lt 0x10000000 -and $section.Address -ne $section.LoadAddress -and $section -eq $sections['.data'])) {
                $flash += $section.Bytes
            }
        }
        $ram = $sections['.data'].Bytes + $sections['.bss'].Bytes
        $measurement = [pscustomobject]@{ Stage=$Stage; Profile=$profile; ExitCode=$buildExit; Flash=$flash; StaticRam=$ram; DisplayBuffers=$typeSizes['size_display_buffers']; RendererHeap=$typeSizes['size_renderer_heap']; AssetResources=$typeSizes['size_asset_resources']; Accepted=($buildExit -eq 0) }
        $measurements += $measurement
        $measurement | Format-List | Out-String | Write-Host
        if ($buildExit -eq 0) {
            $imageBytes = (Get-Item -LiteralPath (Join-Path $buildRoot "$profile/firmware.bin")).Length
            if ($imageBytes -ne $flash) { throw "Map Flash bytes do not match the actual binary: $flash vs $imageBytes" }
            Copy-Item -LiteralPath (Join-Path $buildRoot "$profile/firmware.elf") -Destination (Join-Path $stageRoot "$profile.elf")
        } elseif (-not (Select-String -LiteralPath $logPath -Pattern 'overflowed|region .*overflow|exceeds available' -Quiet)) {
            throw "Build failed for a reason other than capacity; inspect $logPath"
        }
    }
    $measurements | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $stageRoot 'sizes.json') -Encoding utf8
}
finally { Pop-Location }

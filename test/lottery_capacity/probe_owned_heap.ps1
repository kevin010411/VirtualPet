[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$MeasurementRoot,
    [string]$SourceRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$sourcePath = [IO.Path]::GetFullPath($SourceRoot).Replace('\', '/')
$evidencePath = [IO.Path]::GetFullPath($MeasurementRoot)
$compilerRoot = "$env:USERPROFILE/.platformio/packages/toolchain-gccarmnoneeabi/bin"
$measurements = @(Get-Content -LiteralPath (Join-Path $evidencePath 'sizes.json') -Raw | ConvertFrom-Json)
$configuration = Get-Content -LiteralPath (Join-Path $evidencePath 'platformio.ini')
$flags = @{}
$section = ''
foreach ($line in $configuration) {
    if ($line -match '^\[(.+)\]$') { $section = $Matches[1] }
    elseif ($line -match '^build_flags\s*=\s*(.+)$') { $flags[$section] = $Matches[1] }
}
$probe = Join-Path $evidencePath 'owned-heap.cpp'
$rendererSource = Get-Content -LiteralPath "$sourcePath/src/display/Renderer.cpp" -Raw
$rendererState = [regex]::Match($rendererSource, '(?ms)^struct Renderer::AnimationState\b.*?^};').Value
if (-not $rendererState) { throw 'Renderer state definition was not found.' }
$gameHeader = Get-Content -LiteralPath "$sourcePath/include/controller/Game.h" -Raw
$controllerMembers = @{
    heap_animation='animations'; heap_evolution='evolution'; heap_behavior='petBehaviorRuntime'
    heap_executor='commandExecutor'; heap_commands='commands'; heap_layout='layout'
    heap_catalog='collectionCatalog'; heap_appearance='appearanceSelection'; heap_minigame='minigame'
}
@"
#include "display/Renderer.h"
#include "display/FrameDecoder.h"
#include <stddef.h>
#include "animation/AnimationController.h"
#include "appearance/EvolutionController.h"
#include "pet/PetBehaviorRuntime.h"
#include "controller/CommandExecutor.h"
#include "display/LayoutRenderer.h"
#include "collection/CollectionCatalog.h"
#include "appearance/AppearanceSelectionController.h"
#include "controller/MinigameController.h"
$rendererState
char layout_renderer_alignment[alignof(Renderer::AnimationState)];
char layout_line_buffer_offset[offsetof(Renderer::AnimationState, lineBuffer)];
char heap_renderer[sizeof(Renderer::AnimationState)];
char heap_animation[sizeof(AnimationController)];
char heap_evolution[sizeof(EvolutionController)];
char heap_behavior[sizeof(PetBehaviorRuntime)];
char heap_executor[sizeof(CommandExecutor)];
char heap_commands[sizeof(CommandController)];
char heap_layout[sizeof(LayoutRenderer)];
#if ENABLE_COLLECTION_CATALOG
char heap_catalog[sizeof(CollectionCatalog)];
#endif
#if ENABLE_APPEARANCE_SELECTION
char heap_appearance[sizeof(AppearanceSelectionController)];
#endif
#if ENABLE_GUESS_GAME || ENABLE_COMMAND_PREDICT || ENABLE_INSTRUCTION_RESPONSE
char heap_minigame[sizeof(MinigameController)];
#endif
"@ | Set-Content -LiteralPath $probe -Encoding utf8
foreach ($measurement in $measurements) {
    $profile = $measurement.Profile
    $effectiveFlags = $flags["env:$profile"].Replace('${env.build_flags}', $flags['env'])
    $defines = @($effectiveFlags -split '\s+' | Where-Object { $_.StartsWith('-D') })
    $object = Join-Path $evidencePath "$profile-owned-heap.o"
    & "$compilerRoot/arm-none-eabi-g++.exe" -mcpu=cortex-m3 -mthumb -std=c++17 -Os -fno-access-control `
        @defines "-I$sourcePath/test/custom_layout_export" "-I$sourcePath/test/host_stubs" "-I$sourcePath/include" `
        -c $probe -o $object
    if ($LASTEXITCODE -ne 0) { throw "ARM heap type probe failed: $profile" }
    $objects = @{}
    $layout = @{}
    & "$compilerRoot/arm-none-eabi-nm.exe" -S --radix=d $object | ForEach-Object {
        if ($_ -match '^\d+\s+(\d+)\s+B\s+(heap_\w+)$') { $objects[$Matches[2]] = [int]$Matches[1] }
        elseif ($_ -match '^\d+\s+(\d+)\s+B\s+(layout_\w+)$') { $layout[$Matches[2]] = [int]$Matches[1] }
    }
    if ($objects['heap_renderer'] -ne $measurement.RendererHeap) { throw 'Renderer type sizes disagree.' }
    $ownedHeap = @{ heap_renderer = $objects['heap_renderer'] }
    foreach ($key in $controllerMembers.Keys) {
        if ($objects.ContainsKey($key) -and
            $gameHeader -match "std::unique_ptr<[^>]+>\s+$($controllerMembers[$key])\s*;") {
            $ownedHeap[$key] = $objects[$key]
        }
    }
    $total = [int](($ownedHeap.Values | Measure-Object -Sum).Sum)
    $result = [pscustomobject]@{
        Profile = $profile; Objects = $objects; HeapObjects = $ownedHeap; Layout = $layout; FixedOwnedHeap = $total
        StaticPlusFixedOwnedHeap = $measurement.StaticRam + $total
        Defines = $defines
    }
    $result | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath (Join-Path $evidencePath "$profile-owned-heap.json") -Encoding utf8
    Write-Host "$profile : fixed owned heap $total B; static + fixed owned heap $($result.StaticPlusFixedOwnedHeap) B"
}

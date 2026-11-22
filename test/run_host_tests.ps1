[CmdletBinding(DefaultParameterSetName = 'Run')]
param(
    [Parameter(Mandatory, ParameterSetName = 'List')][switch]$List,
    [Parameter(Mandatory, ParameterSetName = 'Core')][switch]$Core,
    [Parameter(Mandatory, ParameterSetName = 'Run')][string[]]$Case,
    [Parameter(ParameterSetName = 'Run')][hashtable]$RunnerOptions = @{}
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$cases = [ordered]@{
    'animation' = 'animation_scene_playback'
    'bundle-reader' = 'bundle_reader_on_access'
    'button-input' = 'button_cheat_input'
    'collection' = 'collection_catalog'
    'custom-layout' = 'custom_layout_export'
    'instruction-response' = 'instruction_response'
    'layout-media' = 'layout_media_export'
    'pet-behavior' = 'pet_behavior_runtime'
    'pet-persistence' = 'pet_persistence'
    'random' = 'firmware_random'
    'rtc-clock' = 'rtc_clock'
    'rtc-power' = 'rtc_power'
    'runtime-contract' = 'runtime_contract_loader'
    'runtime-table' = 'runtime_table_behavior'
    'sequence-audio' = 'sequence_audio'
    'startup' = 'game_startup'
    'startup-error-rendering' = 'renderer_startup_error'
    'stat-event-rules' = 'pet_stat_events'
    'storage-sd' = 'pet_storage_sd_instance'
}
$integrationModes = [ordered]@{
    'capture' = 'CaptureGame'
    'collection-cards' = 'Cards'
    'guess' = 'GuessGame'
    'manual-day' = 'ManualDay'
    'project-menus' = 'ProjectMenus'
    'shop' = 'Shop'
    'stat-events' = @('StatEvents', 'ProjectMenus')
    'transactions' = 'TransactionsOnly'
}
foreach ($name in $integrationModes.Keys) {
    $cases[$name] = 'collection_catalog'
}
$coreCases = @(
    'pet-behavior', 'pet-persistence', 'stat-event-rules', 'startup', 'runtime-contract',
    'transactions', 'random', 'button-input', 'rtc-clock', 'rtc-power', 'storage-sd',
    'bundle-reader', 'startup-error-rendering'
)

if ($List) {
    foreach ($name in $cases.Keys) {
        $runner = "test/$($cases[$name])/run_host_test.ps1"
        $flags = if ($integrationModes.Contains($name)) {
            ' -' + ($integrationModes[$name] -join ' -')
        } elseif ($name -eq 'collection') {
            ' -UseRtcClock'
        }
        Write-Output "$name  $runner$flags"
    }
    Write-Output "`n-Core runs: $($coreCases -join ', ')"
    return
}

if ($Core) { $Case = $coreCases }
foreach ($name in $Case) {
    if (-not $cases.Contains($name)) {
        throw "Unknown host case '$name'. Run test/run_host_tests.ps1 -List."
    }
}
if ($Case.Count -gt 1 -and $RunnerOptions.Count -gt 0) {
    throw 'Run one case at a time when specifying -RunnerOptions.'
}
if (-not (Get-Command g++ -ErrorAction SilentlyContinue)) {
    throw 'A native Windows g++ with C++17 support must be on PATH.'
}

$repoRoot = Split-Path -Parent $PSScriptRoot
$collectionFixture = Join-Path $repoRoot '../../web/tests/fixtures/collection_catalog_v10/valid'
function Get-CaseOptions($name) {
    $options = $RunnerOptions.Clone()
    foreach ($flag in $integrationModes[$name]) {
        if ($options.ContainsKey($flag)) {
            throw "Case '$name' already selects -$flag."
        }
        $options[$flag] = $true
    }
    if ($name -eq 'collection') {
        if (-not $options.ContainsKey('FixtureRoot')) {
            $options['FixtureRoot'] = $collectionFixture
        }
        if (-not $options.ContainsKey('UseRtcClock')) {
            $options['UseRtcClock'] = $true
        }
    }
    if ($name -eq 'transactions' -and -not $options.ContainsKey('FixtureRoot')) {
        # The legacy runner takes a directory argument even when the case uses only in-memory data.
        $options['FixtureRoot'] = $repoRoot
    }
    if ($cases[$name] -eq 'collection_catalog' -or $name -eq 'sequence-audio') {
        if (-not $options.ContainsKey('FixtureRoot') -or
            -not (Test-Path -LiteralPath $options['FixtureRoot'] -PathType Container)) {
            throw "Case '$name' requires -RunnerOptions @{ FixtureRoot = '<extracted Web fixture directory>' }."
        }
        $options['FixtureRoot'] = (Resolve-Path -LiteralPath $options['FixtureRoot']).Path
    }
    if ($name -in @('capture', 'project-menus', 'shop', 'stat-events') -and
        -not (Test-Path -LiteralPath (Join-Path $options['FixtureRoot'] 'runtime.bin') -PathType Leaf)) {
        throw "Case '$name' expects runtime.bin at the root of the extracted fixture: $($options['FixtureRoot'])"
    }
    return $options
}

New-Item -ItemType Directory -Force (Join-Path $repoRoot '.pio') | Out-Null
$failed = @()
$exitCode = 0
foreach ($name in $Case) {
    Write-Host "=== $name ==="
    try {
        $options = Get-CaseOptions $name
        $runner = Join-Path $PSScriptRoot "$($cases[$name])/run_host_test.ps1"
        & $runner @options
        $result = $LASTEXITCODE
    } catch {
        Write-Host $_.Exception.Message
        $result = 1
    }
    if ($result -ne 0) {
        Write-Host "[FAIL] $name (exit $result)"
        $failed += $name
        if ($exitCode -eq 0) { $exitCode = $result }
    } else {
        Write-Host "[PASS] $name"
    }
}
Write-Host "$($Case.Count - $failed.Count)/$($Case.Count) host cases passed."
if ($failed.Count) { Write-Host "Failed: $($failed -join ', ')" }
exit $exitCode

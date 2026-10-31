[CmdletBinding()]
param([string]$LibraryRoot = "", [switch]$AllFeatures, [switch]$Independent, [string]$Pio = "$env:USERPROFILE/.platformio/penv/Scripts/pio.exe")
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$repoRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$outputRoot = Join-Path $repoRoot '.pio/lottery-capacity'
New-Item -ItemType Directory -Force $outputRoot | Out-Null
$configPath = Join-Path $outputRoot 'platformio.ini'
$dependencies = if ($LibraryRoot) {
    "lib_extra_dirs = $([IO.Path]::GetFullPath($LibraryRoot))"
} else {
    "lib_deps =`n    adafruit/Adafruit GFX Library @ ^1.11.5`n    adafruit/Adafruit ST7735 and ST7789 Library`n    adafruit/SdFat - Adafruit Fork@^2.2.54"
}
$config = @"
[platformio]
src_dir = $($repoRoot.Replace('\', '/'))/src
include_dir = $($repoRoot.Replace('\', '/'))/include
build_dir = $($outputRoot.Replace('\', '/'))/build
[env]
platform = ststm32
board = genericSTM32F103C8
framework = arduino
build_flags = -Os -flto -ffunction-sections -fdata-sections -fno-exceptions -DENABLE_DEBUG=0 -DAPP_MAX_PET_STATS=12 -DAPP_MAX_SCREEN_RULES=1 -DENABLE_STARTUP_ANIMATION=1
build_unflags = -Og -O0 -O1 -O2 -O3
$dependencies
[env:lottery_baseline]
build_flags = `${env.build_flags} -DAPP_MAX_ACTION_OUTCOMES=1 -DAPP_MAX_ACTION_EFFECTS=1
[env:lottery_max]
build_flags = `${env.build_flags} -DAPP_MAX_ACTION_OUTCOMES=224 -DAPP_MAX_ACTION_EFFECTS=12
[env:lottery_independent]
build_flags = `${env.build_flags} -DAPP_MAX_ACTIONS=16 -DAPP_MAX_ACTION_OUTCOMES=248 -DAPP_MAX_ACTION_EFFECTS=12
[env:lottery_allfeatures]
build_flags = `${env.build_flags} -DAPP_MAX_ACTION_OUTCOMES=224 -DAPP_MAX_ACTION_EFFECTS=12 -DENABLE_SEQUENCE_AUDIO=1 -DENABLE_GUESS_GAME=1 -DENABLE_COMMAND_OUTFIT=1 -DENABLE_APPEARANCE_SELECTION=1 -DENABLE_COLLECTION_CATALOG=1 -DENABLE_INSTRUCTION_RESPONSE=1 -DENABLE_COMMAND_PREDICT=1 -DENABLE_DEATH=1 -DENABLE_FIRST_START_ANIMATION=1 -DENABLE_GUESS_GAME_SINGLE_ROUND=1 -DENABLE_GUESS_GAME_PLAYER_CHOICE_RESULT=1
"@
Set-Content -LiteralPath $configPath -Value $config -Encoding utf8
$environments = @('-e', 'lottery_baseline', '-e', 'lottery_max')
if ($Independent) { $environments = @('-e', 'lottery_independent') }
if ($AllFeatures) { $environments += @('-e', 'lottery_allfeatures') }
Push-Location $repoRoot
try {
    & $Pio run -c $configPath @environments
    exit $LASTEXITCODE
}
finally { Pop-Location }

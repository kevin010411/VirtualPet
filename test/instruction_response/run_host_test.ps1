[CmdletBinding()]
param([string]$FixtureRoot, [string]$UnusedFixtureRoot, [ValidateSet('valid', 'minimum', 'weighted', 'effects', 'hard', 'nogo', 'maximum')][string]$FixtureKind = 'valid')
$ErrorActionPreference = 'Stop'
$repoRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
Push-Location $repoRoot
try {
    New-Item -ItemType Directory -Force '.pio' | Out-Null
    & g++ -std=c++17 -Wall -Wextra -Werror -DENABLE_INSTRUCTION_RESPONSE=1 -Iinclude test/instruction_response/test_main.cpp src/game/InstructionResponse.cpp -o .pio/instruction_response_host.exe
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    & ./.pio/instruction_response_host.exe
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    & g++ -std=c++17 -Wall -Wextra -Werror -DENABLE_INSTRUCTION_RESPONSE=1 -Itest/button_cheat_input -Iinclude test/instruction_response/test_input.cpp src/platform/ButtonInput.cpp src/game/InstructionResponse.cpp -o .pio/instruction_input_host.exe
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    & ./.pio/instruction_input_host.exe
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    if ($FixtureRoot) {
        $runtimeSources = @('src/resources/RuntimeTableBehavior.cpp',
            'src/resources/RuntimeTableBehaviorDecoder.cpp', 'src/resources/RuntimeTablePresentationDecoder.cpp',
            'src/resources/RuntimeTableReader.cpp',
            'src/resources/RuntimeTableFile.cpp', 'src/appearance/RuntimeTableAppearance.cpp',
            'src/pet/PetBehaviorRuntimeRules.cpp', 'src/pet/RuntimeValueResolver.cpp',
            'src/controller/StatusSetContract.cpp', 'src/controller/SystemCommandCatalog.cpp')
        & g++ -std=c++17 -O1 -DENABLE_INSTRUCTION_RESPONSE=1 -Itest/host_stubs -Iinclude test/instruction_response/test_export.cpp src/game/InstructionResponse.cpp @runtimeSources -o .pio/instruction_export_host.exe
        if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
        & ./.pio/instruction_export_host.exe (Join-Path $FixtureRoot 'runtime.bin') $FixtureKind
        if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
        & g++ -std=c++17 -O1 -DENABLE_INSTRUCTION_RESPONSE=1 -Itest/custom_layout_export -Itest/host_stubs -Iinclude test/instruction_response/test_render.cpp @runtimeSources src/resources/BundleReader.cpp src/resources/AssetRuntimeContract.cpp src/common/CopyResourceName.cpp src/display/FrameDecoder.cpp -o .pio/instruction_render_host.exe
        if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
        & ./.pio/instruction_render_host.exe $FixtureRoot $FixtureKind
        if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    }
    if ($UnusedFixtureRoot) {
        if (-not $FixtureRoot) { throw 'UnusedFixtureRoot requires an enabled FixtureRoot for mismatch proof.' }
        & g++ -std=c++17 -O1 -DENABLE_INSTRUCTION_RESPONSE=0 -Itest/host_stubs -Iinclude test/instruction_response/test_unused.cpp @runtimeSources -o .pio/instruction_unused_host.exe
        if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
        & ./.pio/instruction_unused_host.exe (Join-Path $UnusedFixtureRoot 'runtime.bin') (Join-Path $FixtureRoot 'runtime.bin')
        if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    }
    exit 0
} finally { Pop-Location }

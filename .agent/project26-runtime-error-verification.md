# project_26 runtime resource error — 2026-10-07

## Cause and change

The SD copy at `C:\Users\kevin\Downloads\tmp` matches all 22 exported files in `project-26-sd-card` by SHA-256. Both v14 saves have valid CRCs and select species 18 / outfit 1. The bundle's evolution rules contain six predicates (stage_days plus five Pet Stats). `readEvolutionRecord` rejected counts greater than four. The first evolution query through `SdAppearanceLoader` therefore returned LoadFailed with resource `runtime`, even though the current species did not yet match any evolution rule.

Removed the obsolete four-predicate guard. Counts remain uint8 on the wire, are evaluated one at a time, and must fit within the EvolutionConditions section. Animation references, modes, playback counts, reserved bytes and contiguous child ranges remain checked. No new allocation or larger buffer.

## Evidence

- Before fix, `.pio/project26_evolution_probe.exe C:\Users\kevin\Downloads\tmp` exited 12 and printed `day=0 evolution=2 target=0/0 resource=runtime`.
- After fix, the same probe passed on both supplied folders: days 0 through 10 return NoTarget with an empty error resource; startup and pack decoding pass.
- `test/runtime_table_behavior/run_host_test.ps1` passes. Added real SD appearance-loader regression cases for 5, 6 and 255 predicates, an unmatched sixth predicate, and a condition range extending beyond the section. The pre-fix firmware fails the new test.
- `test/game_startup/run_host_test.ps1 -EnableFirstStartAnimation` passes, including startup, restore, reset, evolution and fatal routing.
- `platformio run -e project_26` passes. Same release setting: Flash 61,448 -> 61,440 / 65,536 bytes; static RAM stays 6,964 / 20,480 bytes.
- `git diff --check` passes. Existing platformio.ini, platformio.base.ini and .gitignore changes were preserved.
- Web preflight passes. Web canonical verification is recorded in `E:\C++\virtualPet\web\.scratch\project26-web-verification.log`; initial sandbox run failed to open temporary SQLite databases and was retried in the host environment.

The diagnostic source is retained under `.scratch/project26-runtime-error/probe.cpp`; it is not production code. No firmware upload, SD write, or save deletion was performed. Physical SD/TFT playback after reflashing remains unverified.

## Canonical Web verification final result

Host-environment retry completed successfully:

```text
Architecture: backend contracts passed
Backend: 309 tests passed
Frontend: 61 files / 509 tests passed
Build: production bundle passed
```

# Firmware refactor analysis handoff — 2026-09-24

## Purpose

Continue a step-by-step discussion of the read-only architecture and performance analysis of `E:\C++\virtualPet\main\code`. The user asked to record the analysis first, then discuss decisions one at a time. No refactor or code edit has been authorized by that request.

## Authoritative context

- Repository: `E:\C++\virtualPet\main\code`; last checked HEAD `d141db0` (2026-09-23 23:19 +0800).
- Cross-layer Pet Stats, behavior, Status, Evolution, persistence, and SD semantics: `E:\C++\virtualPet\web\.scratch\sd-driven-pet-stats\spec.md`. Read before changing those domains; do not copy that spec into firmware docs.
- `AGENTS.md` prefers codebase-memory-mcp graph tools for code discovery. Indexed project name was `E-C-virtualPet-main-code`; graph was ready during analysis. Recheck index freshness and source at the next session.
- Main evidence: `platformio.ini`, `src/platform/main.cpp`, `src/presentation/application/Game.cpp`, `src/pet_behavior/domain/RuntimeTableBehavior.cpp`, `src/shared/assets/BundleReader.cpp`, `src/presentation/adapters/rendering/FrameDecoder.cpp`, `src/presentation/adapters/rendering/Renderer.cpp`, `src/pet/adapters/PetStorage.cpp`, and their headers.

## Findings from the completed analysis

- Target is STM32F103C8, Arduino/PlatformIO, default env `project_12`. Build already uses `-Os`, LTO, function/data sections, and `-fno-exceptions`.
- `main.cpp` owns global hardware, power, sleep, input, and `Game` objects. `Game` composes behavior, commands, appearance, animation, rendering, and persistence. Button interrupts wake STOP mode; interpretation occurs in the cooperative loop. No application threads/tasks/coroutines were found.
- `runtime.bin` is decoded into fixed-capacity `PetBehaviorConfig`; device load avoids a second large config on stack. `PetStorage` uses CRC-protected dual 64-byte save slots. `AnimationController` owns a fixed 8-entry FIFO. User Action values are committed before animation availability is checked; preserve this contract.
- `BundleReader::openFrame` closes/reopens a pack per frame, reads its header, resolves animation (with one-entry cache), reads descriptor, then `FrameDecoder` streams through a 1,024-byte read buffer and a 128×12 RGB565 line buffer (3,072 bytes). Rendering writes batches to TFT; there is no full-frame RAM buffer.
- Startup and appearance queries repeatedly open `runtime.bin` and read its envelope. This is a plausible SD-I/O opportunity, but existing validation and first-error behavior must be preserved. Frame delay must be measured as SD open/index/read, decode, and TFT transfer separately.
- `Game` uses several startup-time `unique_ptr` allocations that live for the session; Renderer allocates its buffer state once. Moving them to static storage alone will not reduce total RAM. Current peak stack, heap, allocation count, FPS, and latency were not measured.
- Existing `.pio/build/project_12/firmware.elf` predates HEAD. It showed `.text` 56,792 B, `.rodata` 4,464 B, `.data` 240 B, `.bss` 6,600 B, and 1,536 B linker minimum heap/stack reserve. The 61,792 B Flash-load sum and 6,840 B static RAM sum are **stale artifact reference values**, not a current baseline.
- Running PlatformIO was blocked by permissions on the global `.platformio` lock/cache; pointing its core directory at repo `.piohome` also failed ownership checks. No current build or device benchmark ran.

## Proposed discussion order

1. **Measurement baseline:** agree on representative `project_12` SD bundle, target board, scenarios, and acceptance metrics. First obtain a current linked build, map, startup timing, frame-stage timing, heap high-water, and stack watermark. Do not claim optimization from the stale ELF.
2. **Startup runtime.bin reads:** trace exact repeated reads and first-error semantics; decide whether to pass an already-validated manifest and consolidate appearance validation. Keep changes isolated and host-testable.
3. **Renderer RAM:** compare current 12-line batch with 6-line batch. The buffer formula gives a 1,536-byte allocation difference; measure frame latency and verify pixels before adopting.
4. **Per-frame pack access:** benchmark pack open/header/index and consider retaining a validated handle only during continuous playback, with explicit invalidation on resource or flow transitions.
5. **Ownership and Flash:** inspect one-time heap allocations, linker map, and actual reachable library symbols; A/B link each candidate. Avoid broad rewrite or new framework.

For each item, discuss evidence, proposed seam, compatibility risks, validation, and rollback before any implementation. The previous response included a 13-section As-Is/To-Be analysis in this conversation; use it for fuller rationale rather than duplicating it here.

## Validation limits and workspace state

- Read-only source and old ELF inspection only; no tests, hardware runs, or current build.
- The prior PlatformIO attempt deleted tracked `.piohome/.cache/telemetry.json`. It was restored with the required filesystem escalation. Recheck `git status` before implementation.

## Suggested skills

- `codebase-design` for module interfaces, ownership, and seam decisions.
- `diagnosing-bugs` only if the PlatformIO permissions or a later build failure must be diagnosed.

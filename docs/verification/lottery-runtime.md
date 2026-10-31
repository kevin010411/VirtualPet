# Lottery runtime software evidence — 2026-10-31

Implementation base: `b130bda2c3acae08a816382b3036550c9abc06af`. Paired Web exporter emits Runtime Table v27. Existing SD bundles must be re-exported; no legacy parser was added. No source profile was overwritten and no device was flashed.

## Runtime bounds

The result table has at most 224 rows (eight Actions × 28 results), uint8 outcome indices and uint16 effect ranges. Effects retain full signed64 operands and unsigned32 Stat values. Exported profile macros `APP_MAX_ACTION_OUTCOMES` and `APP_MAX_ACTION_EFFECTS` size local storage; a maximum project uses 224 descriptors and 12 local effect entries. When total effects exceed local entries, only the selected result's at most12 effect records are read from `/runtime.bin`, into the existing temporary Stat calculation. Any read failure leaves Stats, pauses and tracked counts unchanged; later animation failure retains committed effects. The reader stores a byte offset rather than a pointer to a closed parser Source.

ACTION_OUTCOMES remains10bytes: the final uint16 is0 for ordinary random Version choice, otherwise exact Version index+1. ACTION_PLAYBACKS final byte1 marks an optional prelude; byte0 retains following segments. Existing queue8 accommodates prelude + primary + three following segments. Selection, effect commit and fixed replay share one chosen result.

ARM Cortex-M3 sizeof probe: AnimationRef4, PlaybackConfig8, ActionConfig18, OutcomeConfig14, EffectConfig16. Maximum config with12Stats, screenrule1 and optional featuresOFF is6888bytes. Simply expanding old fixed effects would require43008bytes for effects alone; this implementation avoids that allocation.

## Executed hosts

- `test/pet_behavior_runtime/run_host_test.ps1`: existing Stats6/10/12 rules and CatalogOFF/ON execution passed. Additional maximum-profile run covers all224results and both ends of each deterministic weight range, all12effects, exact Version correspondence and atomic final-record failure. Execution coverage verifies prelude/result/following order, fixed Version27 repeating three times, retained effects on missing Version, actual SD stub reads, truncated file and unavailable SD without partial commit.
- `test/game_startup/run_host_test.ps1 -DeathOnly -DeathEnabled 1`: passed saved Dead, gating, reset/save rollback and new life.
- `test/runtime_table_behavior/run_host_test.ps1 -LotteryFixture <Web maximum runtime.bin>`: actual Editor API save/export fixture passed224fixed-Version results and2688effects; selected Stat values agree with actual exported bytes. The first fixture emitted only one Outfit because appearance selection was disabled. After correcting the generator to enable appearance selection, the re-export contains eight OUTFITS records; the actual reader passed all224 result mappings in all eight Outfits. This supersedes the earlier one-Outfit evidence.

## Linked target

Actual board: PlatformIO `genericSTM32F103C8`, Cortex-M3,20KiB RAM/64KiB Flash; STM32 platform19.0.0, Arduino2.9.0, GCC12.3.1. Shared options:12Stats, screenrule1, startupON, `-Os -flto`, debugOFF; generated local config does not replace `platformio.ini`.

| Profile | Outcome/effect local entries | RAM | Flash | Result |
|---|---:|---:|---:|---|
| Baseline |1/1|7004/20480|59996/65536|linked|
| Maximum lottery |224/12|10292/20480|60404/65536|linked|
| Maximum + all optional features |224/12|not accepted|overflow10072|link rejected|

Maximum lottery costs3288static RAM bytes and408Flash bytes versus the same-feature baseline. These are linked static allocation results, not peak-RAM measurements. Alloptionalfeatures includes sequence audio, GuessGame, Predict, collection, appearance/outfit, instruction response, death and FirstStart; its `.text` overflow is a concrete capability gap, not a passed configuration. The failed map has data+bss15288bytes but is not an accepted firmware image. An allfeatures no-lottery baseline was not measured; the overflow cannot be attributed entirely to lottery.

A rerunnable profile generator/build entry is `test/lottery_capacity/run_target_build.ps1 [-LibraryRoot <cached compatible libraries>] [-AllFeatures]`. It writes only `.pio/lottery-capacity` and preserves the user's ini. This script was syntax-reviewed; the measurements above came from the equivalent generated `.pio/lottery-target.ini` profile used during development, with final baseline/max incremental builds rerun after source edits.

Pending: ordinary v27 full fixture hosts after canonical Web fixture generation; simultaneous maximum screen/Status/other feature content, allfeatures Flash feasibility, peak stack/heap, and physical SD/TFT/button/death/interruption acceptance. Hosts and links do not establish device acceptance.

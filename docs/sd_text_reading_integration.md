# Unified SD Text Reading Integration

This note records the firmware-specific integration boundary for the schema-v9
SD text-reading path. The authoritative cross-layer behavior remains in the Web
repository at
`E:\C++\virtualPet\web\.scratch\sd-driven-pet-stats\spec.md`.

The integrated implementation is the contiguous prerequisite commit range
`0fbb91d^..4397055`: Runtime Contract loading, cached Status Sets, Evolution,
Layout Index, and animation and appearance manifest migrations. This note does
not replace those implementation changes.

## Integrated paths

- `/runtime_contract.txt` is loaded once during `Game::setup_game()` through
  `loadPetBehaviorContract()`. Pet Behavior and Status Set records share that
  file's identity, version, and whole-file CRC before the decoded configuration
  is committed.
- Status command execution reads `PetBehaviorConfig::statusSets` from RAM. It
  does not reopen a Pet Behavior or Status configuration file.
- Evolution keeps `/evolution_rules.txt`, Layout keeps `/layout/index.txt`, and
  animation and appearance manifests keep their existing paths and formats.
  Their line-oriented reads use `loadSdDelimitedTextRecords()`.
- `SdTextRecordReader.cpp` owns the line-oriented SD byte loop. Reader buffers
  are local to each load and are not retained by `Game`, `Renderer`, or the
  normal rendering path.
- Current firmware source contains no `pet_behavior.txt` or `status_sets.txt`
  fallback path. The remaining direct `File::available()` outside the shared
  text reader belongs to the binary RLE frame decoder, not SD text parsing.

## Static inspection basis

- Legacy filename reachability: current `include/`, `src/`, `test/`, and `docs/`
  were searched for `pet_behavior.txt` and `status_sets.txt`.
- Duplicate byte-loop reachability: current source was searched for
  `readStringUntil`, `readBytesUntil`, `File::available()`, and `File::read()`.
- Migrated call sites were inspected in `PetBehaviorContract.cpp`,
  `SdAppearanceLoader.cpp`, `LayoutRenderer.cpp`, and `AssetManifest.cpp`.
- Cached Status execution was inspected in `CommandExecutor.cpp`; its Status Set
  selection reads the startup `PetBehaviorConfig` and performs no SD open.

## Verification boundary

Static source reachability was inspected for the integration described above.
Automated tests, canonical Backend verification, PlatformIO profile builds, and
device workflows were not run for this integration record.

The remaining runtime boundary is therefore all executable behavior: startup
and CRC rejection, arbitrary Runtime Contract record order, cached Status
selection, Evolution, dynamic Layout, manifest override and reload behavior,
appearance selection and previews, profile RAM and Flash fit, and device-level
startup/display interaction.

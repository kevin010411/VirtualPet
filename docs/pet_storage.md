# Pet Storage Format

Pet state is stored on the SD card in two alternating slots:

```txt
/state_a.bin
/state_b.bin
```

The firmware no longer reads the old `/state.bin` or `/state.bak` files.

## Record

Each slot contains one binary `PersistedPetState` record:

- `magic`
- `version`
- `sequence`
- `schemaFingerprint`
- `species`
- `outfit`
- `stage_days`
- `customStats[8]` (`custom0` through `custom7`)
- `flowFlags`
- `crc32`

`version` is currently `12`. Fixed care values, age-tenths, sickness, health,
and derived health state are not part of runtime or persistent pet state.
Project-defined Pet Stats are stored in the fixed `customStats` slots.

`schemaFingerprint` is emitted by the authoritative Web Pet Stat Slot contract
at `E:\C++\virtualPet\web\.scratch\sd-driven-pet-stats\spec.md`. Firmware does
not reconstruct it. A mismatch discards both save slots instead of migrating
values into a new schema; a match retains the saved Pet State.

`flowFlags` stores app-flow state. Bit `0` marks the first-launch flow as
complete.

## CRC

`crc32` is calculated with a bitwise CRC32 implementation over the record bytes
from the beginning of the struct up to, but not including, the `crc32` field.

No lookup table is used, keeping flash and RAM overhead small.

## Load

On boot, firmware reads both slots:

1. Reject slots with the wrong file size.
2. Reject slots with the wrong `magic` or `version`.
3. Reject slots whose CRC32 does not match.
4. Load the valid slot with the newest `sequence`.

If neither slot is valid, the pet starts from the default state.

## Save

After loading, firmware writes the next save to the older slot. Each successful
save increments `sequence` and alternates the target slot.

This means a power loss during a save can corrupt the slot being written, but
the previous valid slot should remain available for the next boot.

## Compatibility

The legacy single-slot `/state.bin` and `/state.bak` files and earlier dual-slot
record versions remain unsupported. A version or schema mismatch starts a new
pet through the normal First Launch flow.

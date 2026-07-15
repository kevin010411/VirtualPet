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
- `hasSick`
- `ageTenths`
- `hungry_value`
- `mood`
- `clean_value`
- `env_value`
- `species`
- `outfit`
- `stage_days`
- `health`
- `customStats[8]` (`custom0` through `custom7`)
- `flowFlags`
- `crc32`

`version` is currently `10`. Age is stored as an unsigned integer in tenths of a
year (`1000` means `100.0` years), so normal firmware operation does not need
floating-point support.

`status` is not persisted. It is derived whenever needed from hunger, mood,
environment cleanliness, pet cleanliness, sickness, and the active `PetConfig`.
Enum values such as `Healthy`, `Hungry`, and `Sick` therefore never become stale
when thresholds change between firmware builds.

`health` remains persisted because it is a cumulative stage score updated over
time; it cannot be reconstructed from the current status fields. The same is
true for `stage_days` and `custom0` through `custom7`.

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

The legacy single-slot `/state.bin` and `/state.bak` files remain unsupported.
Version `7`, `8`, and `9` dual-slot records are accepted once. Version `7` age values
are converted from IEEE-754 to tenths without adding floating-point support.
The old stage healthy-day count initializes both `stage_days` and `health`, with
`health` clamped to `0..100`. Version `9`'s stored `status` is discarded. The
next save writes the compact version `10` record.

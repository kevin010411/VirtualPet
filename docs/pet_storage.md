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
- pet status fields
- `species`
- `outfit`
- `healthy_days`
- `crc32`

`version` is currently `4`.

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

Old state formats are intentionally unsupported. To reset a device that still
has old state files, remove `/state.bin` and `/state.bak` or leave them in place;
the current firmware ignores them and creates `/state_a.bin` and `/state_b.bin`
on future saves.

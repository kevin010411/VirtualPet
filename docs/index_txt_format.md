# `index.txt` Format Specification

`index.txt` is the animation manifest stored on the SD card. It is the single source of truth for animation resources.

## File location

Recommended path:

```txt
/index.txt
```

## Design goals

- One line per animation resource
- Very simple MCU parsing
- Human-readable and easy to edit by hand
- Supports deep folder structures without changing firmware
- Keeps `bmp`, `raw`, and `rle` resources on the same schema
- Supports reusable path templates for multiple appearances that share the same asset layout

## Line format

Each non-empty line must use this fixed field order:

```txt
id|format|frames|width|height|fps|path
```

Example:

```txt
Idle|bmp|6|128|96|6|/{species}/{outfit}/idle
```

Lines starting with `#` are comments. Empty lines are ignored.

## Field definitions

- `id`
  - Must match the firmware animation id name exactly
  - Example: `Idle`, `Hungry`, `Feed`, `GuessWin`
- `format`
  - Supported values:
  - `bmp`
  - `raw`
  - `rle`
- `frames`
  - Total frame count
  - Must be greater than `0`
- `width`
  - Frame width in pixels
  - Must be greater than `0`
- `height`
  - Frame height in pixels
  - Must be greater than `0`
- `fps`
  - Animation playback FPS for this animation
  - The firmware converts this to a frame interval with `1000 / fps` milliseconds
  - `0` means use the firmware default frame interval
  - This controls playback cadence only; the drawing/decoding path is still selected by `format`
- `path`
  - Folder or base path of the animation asset
  - Supports `{species}` and `{outfit}` tokens, which are replaced by the firmware-selected short codes
  - Also accepts legacy `{animal}` as a species alias
  - Firmware will load frames as:
  - `path/1.bmp`, `path/2.bmp`, ... for `bmp`
  - `path/1.raw`, `path/2.raw`, ... for `raw`
  - `path/1.rle`, `path/2.rle`, ... for `rle`

## Validation rules

- Every resource line must contain exactly 7 fields
- Unknown `id` values are ignored
- Unknown `format` values are ignored
- `frames`, `width`, or `height` equal to `0` are ignored
- Empty `path` values are ignored
- `fps` may be `0`; non-zero values should be chosen so the target hardware can finish drawing each frame before the next interval

## Playback behavior

`fps` can be tuned independently per animation. For example, a status loop can use `6` FPS while a short result animation uses `10` FPS.

The field does not switch rendering logic. Rendering is selected by `format`:

- `bmp` uses numbered BMP frames and the BMP decoder
- `raw` uses numbered RGB565 raw frames
- `rle` uses numbered RGB565 RLE frames

If a non-zero `fps` is configured, both normal gameplay animation and the low-battery animation use that manifest value. If the manifest line is missing or `fps` is `0`, firmware falls back to its default frame interval.

## Example

```txt
# Pet base status
Idle|bmp|6|128|96|6|/{species}/{outfit}/idle
Hungry|bmp|4|128|96|6|/{species}/{outfit}/hungry
Depress|bmp|4|128|96|6|/{species}/{outfit}/depress

# Command animations
Feed|raw|5|128|96|8|/{species}/{outfit}/feed
Gift|rle|6|128|96|8|/{species}/{outfit}/gift

# Minigame animations
GuessStart|bmp|2|128|96|8|/{species}/{outfit}/guess_start
GuessItem4|bmp|4|128|96|8|/{species}/{outfit}/item4
GuessLL|bmp|2|128|96|8|/{species}/{outfit}/ll
GuessLR|bmp|2|128|96|8|/{species}/{outfit}/lr
GuessRL|bmp|2|128|96|8|/{species}/{outfit}/rl
GuessRR|bmp|2|128|96|8|/{species}/{outfit}/rr
GuessWin|raw|4|128|96|10|/{species}/{outfit}/guess_win
GuessLoss|rle|4|128|96|10|/{species}/{outfit}/guess_loss

# System animations
Battery|bmp|2|128|96|6|/battery
```

## Folder layout recommendation

```txt
/index.txt
/dino/base/idle/1.bmp
/cat/base/hungry/1.bmp
/dog/hat/feed/1.raw
/dog/hat/guess_win/1.rle
/battery/1.bmp
/assets/ui/layout/1.bmp
```

## Appearance usage

If the SD card contains:

```txt
/dino/base/idle/1.bmp
/dino/hat/idle/1.bmp
/drgn/base/idle/1.bmp
```

Then the same manifest line:

```txt
Idle|bmp|6|128|96|6|/{species}/{outfit}/idle
```

can be reused by changing the firmware-side selected appearance codes, for example:

- `species=dino`, `outfit=base`
- `species=dino`, `outfit=hat`
- `species=drgn`, `outfit=base`

Keep `species` and `outfit` codes at 8 characters or fewer. Use `outfit=base` when no outfit is equipped.

## Guess item result naming

Guess item result animations use `GuessXY`, where:

- `X` is the pet-facing direction: `L` or `R`
- `Y` is the item position: `L` or `R`

Use matching lowercase folder names on the SD card:

```txt
GuessLL|bmp|2|128|96|8|/{species}/{outfit}/ll
GuessLR|bmp|2|128|96|8|/{species}/{outfit}/lr
GuessRL|bmp|2|128|96|8|/{species}/{outfit}/rl
GuessRR|bmp|2|128|96|8|/{species}/{outfit}/rr
```

Examples:

- `GuessLL`: pet faces left, item is on the left
- `GuessLR`: pet faces left, item is on the right
- `GuessRL`: pet faces right, item is on the left
- `GuessRR`: pet faces right, item is on the right

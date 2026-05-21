# Renderer Asset Format Notes

This project currently renders BMP image sequences from SD card for compatibility and easy inspection.

Recommended streaming-ready formats for the next pipeline iteration:

1. RGB565 raw frame sequence
- Best match for ST7735-class TFT panels.
- No BMP parsing and no 24-bit to 16-bit conversion at runtime.
- Fastest CPU-side path, but requires external metadata for width, height, and frame count.

2. RLE-compressed RGB565 sequence
- Trades a small decoder cost for reduced SD bandwidth.
- Works well for flat-color scenes or repeated pixel runs.
- Suitable when SD I/O is the bottleneck.

3. Sprite sheet / atlas
- Reduces filesystem overhead by storing many frames in one asset.
- Good when frame dimensions are fixed and assets can be pre-packed offline.
- Can still use BMP during development, but should not remain the long-term high-FPS format.

Current implementation strategy:
- Keep BMP as a fallback.
- Support metadata-driven `raw` RGB565 sequences.
- Support metadata-driven `rle` RGB565 sequences.
- Allow `main.cpp` to choose whether `.bmp` or `.rle` should be preferred when both assets exist.

RLE file layout used by the renderer:

1. File header
- `uint16_t width` in little-endian
- `uint16_t height` in little-endian

2. Pixel stream
- Repeated packets of:
- `uint16_t run_length` in little-endian
- `uint16_t rgb565_color` in little-endian

3. Decoding rule
- Expand each packet into `run_length` pixels of `rgb565_color`
- The expanded pixel count must match `width * height`
- No end marker is used; the stream ends exactly when all pixels are decoded

Notes:
- `bmp` remains the easiest format to inspect manually.
- `rle` is a better runtime choice when frames contain large same-color regions and SD bandwidth is the bottleneck.
- `raw` is still the simplest high-throughput format when asset size is acceptable.

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
- Add metadata-driven support for raw RGB565 sequences.
- Reserve RLE as a future extension of the metadata format.

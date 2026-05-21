#!/usr/bin/env python3
"""
Convert all BMP files under a folder tree into renderer-compatible RLE files.

The output keeps the same relative directory structure as the input tree.

RLE format:
1. uint16 little-endian width
2. uint16 little-endian height
3. Repeated packets of:
   - uint16 little-endian run length
   - uint16 little-endian RGB565 color
"""

from __future__ import annotations

import argparse
import struct
from pathlib import Path


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Recursively convert BMP files to renderer-compatible RLE files.",
    )
    parser.add_argument(
        "input_root",
        type=Path,
        help="Root folder containing BMP files.",
    )
    parser.add_argument(
        "-o",
        "--output-root",
        type=Path,
        help="Root folder for generated RLE files. Defaults to '<input_root>_rle'.",
    )
    parser.add_argument(
        "--overwrite",
        action="store_true",
        help="Overwrite existing .rle files in the output tree.",
    )
    return parser.parse_args()


def read_exact(handle, size: int) -> bytes:
    data = handle.read(size)
    if len(data) != size:
        raise ValueError("unexpected end of file")
    return data


def rgb888_to_rgb565(red: int, green: int, blue: int) -> int:
    return ((red & 0xF8) << 8) | ((green & 0xFC) << 3) | (blue >> 3)


def decode_bmp_pixels(path: Path) -> tuple[int, int, list[int]]:
    with path.open("rb") as handle:
        if read_exact(handle, 2) != b"BM":
            raise ValueError("not a BMP file")

        file_size, _reserved1, _reserved2, pixel_offset = struct.unpack(
            "<IHHI", read_exact(handle, 12)
        )
        if file_size < pixel_offset:
            raise ValueError("invalid BMP header")

        dib_size = struct.unpack("<I", read_exact(handle, 4))[0]
        if dib_size < 40:
            raise ValueError("unsupported DIB header")

        dib_rest = read_exact(handle, dib_size - 4)
        width, height, planes, bits_per_pixel, compression = struct.unpack(
            "<iiHHI", dib_rest[:16]
        )

        if planes != 1:
            raise ValueError("unsupported BMP planes value")
        if bits_per_pixel != 24:
            raise ValueError("only 24-bit BMP is supported")
        if compression != 0:
            raise ValueError("compressed BMP is not supported")
        if width <= 0 or height == 0:
            raise ValueError("invalid BMP dimensions")

        top_down = height < 0
        abs_height = abs(height)
        row_stride = ((width * 3 + 3) // 4) * 4

        handle.seek(pixel_offset)
        rows: list[list[int]] = []
        for _ in range(abs_height):
            row = read_exact(handle, row_stride)
            pixels: list[int] = []
            for x in range(width):
                blue = row[x * 3]
                green = row[x * 3 + 1]
                red = row[x * 3 + 2]
                pixels.append(rgb888_to_rgb565(red, green, blue))
            rows.append(pixels)

        if not top_down:
            rows.reverse()

        flattened = [pixel for row in rows for pixel in row]
        return width, abs_height, flattened


def encode_rle_stream(pixels: list[int]) -> bytes:
    encoded = bytearray()
    index = 0
    total = len(pixels)

    while index < total:
        color = pixels[index]
        run_length = 1
        index += 1

        while index < total and pixels[index] == color and run_length < 0xFFFF:
            run_length += 1
            index += 1

        encoded += struct.pack("<HH", run_length, color)

    return bytes(encoded)


def convert_bmp_to_rle(source: Path, destination: Path, overwrite: bool) -> None:
    if destination.exists() and not overwrite:
        raise FileExistsError(f"destination already exists: {destination}")

    width, height, pixels = decode_bmp_pixels(source)
    payload = bytearray()
    payload += struct.pack("<HH", width, height)
    payload += encode_rle_stream(pixels)

    destination.parent.mkdir(parents=True, exist_ok=True)
    destination.write_bytes(payload)


def iter_bmp_files(root: Path) -> list[Path]:
    return sorted(path for path in root.rglob("*") if path.is_file() and path.suffix.lower() == ".bmp")


def main() -> int:
    args = parse_args()
    input_root = args.input_root.resolve()
    if not input_root.exists() or not input_root.is_dir():
        raise SystemExit(f"input root does not exist or is not a directory: {input_root}")

    output_root = args.output_root.resolve() if args.output_root else input_root.with_name(f"{input_root.name}_rle")
    bmp_files = iter_bmp_files(input_root)
    if not bmp_files:
        print(f"No BMP files found under: {input_root}")
        return 0

    converted = 0
    skipped = 0
    failed = 0

    for bmp_path in bmp_files:
        relative = bmp_path.relative_to(input_root)
        rle_path = output_root / relative.with_suffix(".rle")
        try:
            convert_bmp_to_rle(bmp_path, rle_path, overwrite=args.overwrite)
            converted += 1
            print(f"[OK] {bmp_path} -> {rle_path}")
        except FileExistsError:
            skipped += 1
            print(f"[SKIP] {rle_path} already exists")
        except Exception as exc:  # noqa: BLE001
            failed += 1
            print(f"[FAIL] {bmp_path}: {exc}")

    print(
        f"Done. converted={converted} skipped={skipped} failed={failed} output_root={output_root}"
    )
    return 1 if failed else 0


if __name__ == "__main__":
    raise SystemExit(main())

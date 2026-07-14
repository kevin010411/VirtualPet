#!/usr/bin/env python3
"""Build PlatformIO profiles and report Flash/RAM size deltas.

Flash is measured as text + data.
RAM is measured as data + bss.
"""

from __future__ import annotations

import argparse
import json
import os
import re
import subprocess
import sys
from dataclasses import dataclass
from pathlib import Path
from typing import Iterable


DEFAULT_PROFILES = [
    "default",
    "kuromu",
    "small",
    "new_taipei_childrens_day",
    "dipsyho",
]

SIZE_RE = re.compile(
    r"^\s*(?P<text>\d+)\s+(?P<data>\d+)\s+(?P<bss>\d+)\s+"
    r"(?P<dec>\d+)\s+(?P<hex>[0-9a-fA-F]+)\s+(?P<filename>\S+firmware\.elf)\s*$",
    re.MULTILINE,
)


@dataclass(frozen=True)
class SizeResult:
    profile: str
    text: int
    data: int
    bss: int

    @property
    def flash(self) -> int:
        return self.text + self.data

    @property
    def ram(self) -> int:
        return self.data + self.bss

    def to_json(self) -> dict[str, int]:
        return {
            "text": self.text,
            "data": self.data,
            "bss": self.bss,
            "flash": self.flash,
            "ram": self.ram,
        }


def repo_root() -> Path:
    return Path(__file__).resolve().parents[1]


def default_platformio() -> str:
    configured = os.environ.get("PLATFORMIO_EXE")
    if configured:
        return configured

    home = Path.home()
    candidate = home / ".platformio" / "penv" / "Scripts" / "platformio.exe"
    if candidate.exists():
        return str(candidate)

    return "platformio"


def default_nm() -> str:
    configured = os.environ.get("ARM_NONE_EABI_NM")
    if configured:
        return configured

    packages = Path.home() / ".platformio" / "packages"
    candidates = [
        packages / "toolchain-gccarmnoneeabi" / "bin" / "arm-none-eabi-nm.exe",
        packages / "toolchain-gccarmnoneeabi" / "bin" / "arm-none-eabi-nm",
    ]
    candidates.extend(packages.glob("toolchain-gccarmnoneeabi*/*/arm-none-eabi-nm.exe"))
    candidates.extend(packages.glob("toolchain-gccarmnoneeabi*/*/arm-none-eabi-nm"))
    for candidate in candidates:
        if candidate.exists():
            return str(candidate)

    return "arm-none-eabi-nm"


def parse_size_output(profile: str, output: str) -> SizeResult:
    matches = list(SIZE_RE.finditer(output))
    if not matches:
        raise ValueError(f"Could not find PlatformIO size table for profile '{profile}'.")

    match = matches[-1]
    return SizeResult(
        profile=profile,
        text=int(match.group("text")),
        data=int(match.group("data")),
        bss=int(match.group("bss")),
    )


def run_size(platformio: str, profile: str, root: Path) -> SizeResult:
    command = [platformio, "run", "-e", profile, "-t", "size"]
    completed = subprocess.run(
        command,
        cwd=root,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        check=False,
    )
    sys.stdout.write(completed.stdout)
    if completed.returncode != 0:
        raise RuntimeError(f"PlatformIO size build failed for profile '{profile}'.")
    return parse_size_output(profile, completed.stdout)


def load_baseline(path: Path) -> dict[str, dict[str, int]]:
    with path.open("r", encoding="utf-8") as fh:
        payload = json.load(fh)

    profiles = payload.get("profiles", payload)
    if not isinstance(profiles, dict):
        raise ValueError(f"Invalid baseline format: {path}")
    return profiles


def write_results(path: Path, results: Iterable[SizeResult]) -> None:
    payload = {
        "metrics": {
            "flash": "text + data",
            "ram": "data + bss",
        },
        "profiles": {result.profile: result.to_json() for result in results},
    }
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", encoding="utf-8", newline="\n") as fh:
        json.dump(payload, fh, indent=2, sort_keys=True)
        fh.write("\n")


def signed(value: int) -> str:
    if value > 0:
        return f"+{value}"
    return str(value)


def markdown_table(
    results: list[SizeResult],
    baseline: dict[str, dict[str, int]] | None,
) -> str:
    lines = [
        "| Profile | text | data | bss | Flash text+data | RAM data+bss | Flash delta | RAM delta |",
        "| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |",
    ]
    for result in results:
        base = baseline.get(result.profile) if baseline else None
        flash_delta = "-"
        ram_delta = "-"
        if base:
            flash_delta = signed(result.flash - int(base["flash"]))
            ram_delta = signed(result.ram - int(base["ram"]))
        lines.append(
            f"| `{result.profile}` | {result.text} | {result.data} | {result.bss} | "
            f"{result.flash} | {result.ram} | {flash_delta} | {ram_delta} |"
        )
    return "\n".join(lines)


def regression_failures(
    results: list[SizeResult],
    baseline: dict[str, dict[str, int]] | None,
    fail_on_regression: bool,
    max_flash_delta: int,
    max_ram_delta: int,
) -> list[str]:
    if not baseline or not fail_on_regression:
        return []

    failures: list[str] = []
    for result in results:
        base = baseline.get(result.profile)
        if not base:
            failures.append(f"{result.profile}: missing baseline entry")
            continue

        flash_delta = result.flash - int(base["flash"])
        ram_delta = result.ram - int(base["ram"])
        if flash_delta > max_flash_delta:
            failures.append(
                f"{result.profile}: Flash regression {flash_delta} bytes "
                f"(allowed {max_flash_delta})"
            )
        if ram_delta > max_ram_delta:
            failures.append(
                f"{result.profile}: RAM regression {ram_delta} bytes "
                f"(allowed {max_ram_delta})"
            )
    return failures


def top_bss_symbols(profile: str, count: int, root: Path, nm: str) -> list[str]:
    if count <= 0:
        return []

    elf = root / ".pio" / "build" / profile / "firmware.elf"
    if not elf.exists():
        raise FileNotFoundError(f"Missing ELF for profile '{profile}': {elf}")

    completed = subprocess.run(
        [nm, "-S", "--size-sort", str(elf)],
        cwd=root,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        check=False,
    )
    if completed.returncode != 0:
        raise RuntimeError(completed.stdout)

    rows: list[tuple[int, str]] = []
    for line in completed.stdout.splitlines():
        parts = line.split(maxsplit=3)
        if len(parts) != 4:
            continue
        _, size_hex, kind, name = parts
        if kind.lower() != "b":
            continue
        try:
            rows.append((int(size_hex, 16), name))
        except ValueError:
            continue

    rows = rows[-count:]
    rows.reverse()
    return [f"{size:6d} {name}" for size, name in rows]


def parse_args(argv: list[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Build selected PlatformIO profiles and compare text/data/bss size.",
    )
    parser.add_argument(
        "-e",
        "--profile",
        action="append",
        dest="profiles",
        help="Profile to measure. Can be passed more than once.",
    )
    parser.add_argument(
        "--platformio",
        default=default_platformio(),
        help="Path to platformio executable. Defaults to PLATFORMIO_EXE, user install, then PATH.",
    )
    parser.add_argument(
        "--baseline",
        type=Path,
        help="JSON baseline to compare against.",
    )
    parser.add_argument(
        "--write-json",
        type=Path,
        help="Write measured sizes as a JSON baseline/report.",
    )
    parser.add_argument(
        "--fail-on-regression",
        action="store_true",
        help="Exit non-zero when measured Flash/RAM exceeds the baseline allowance.",
    )
    parser.add_argument(
        "--max-flash-delta",
        type=int,
        default=0,
        help="Allowed Flash increase in bytes when --fail-on-regression is set.",
    )
    parser.add_argument(
        "--max-ram-delta",
        type=int,
        default=0,
        help="Allowed RAM increase in bytes when --fail-on-regression is set.",
    )
    parser.add_argument(
        "--top-bss",
        type=int,
        default=0,
        help="Print the largest .bss symbols for each measured profile.",
    )
    parser.add_argument(
        "--nm",
        default=default_nm(),
        help="Path to nm executable used by --top-bss.",
    )
    return parser.parse_args(argv)


def main(argv: list[str]) -> int:
    args = parse_args(argv)
    root = repo_root()
    profiles = args.profiles or DEFAULT_PROFILES
    baseline = load_baseline(args.baseline) if args.baseline else None

    results: list[SizeResult] = []
    for profile in profiles:
        results.append(run_size(args.platformio, profile, root))

    print()
    print(markdown_table(results, baseline))

    if args.write_json:
        write_results(args.write_json, results)
        print()
        print(f"Wrote size report: {args.write_json}")

    if args.top_bss:
        print()
        for result in results:
            print(f"Top .bss symbols for {result.profile}:")
            for line in top_bss_symbols(result.profile, args.top_bss, root, args.nm):
                print(f"  {line}")

    failures = regression_failures(
        results,
        baseline,
        args.fail_on_regression,
        args.max_flash_delta,
        args.max_ram_delta,
    )
    if failures:
        print()
        print("Size regression detected:")
        for failure in failures:
            print(f"- {failure}")
        return 1

    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))

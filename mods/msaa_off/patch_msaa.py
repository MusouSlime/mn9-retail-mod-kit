#!/usr/bin/env python3
"""Force MSAA off in MN9Game.bin.x86_64.

The game's native UMN9_SystemData::ApplyVideoOptionData computes
    MaxMultiSamples = 1 << VideoOptionData.AntiAliasing
and
    bAllowD3D9MSAA = (AntiAliasing > 0)
from the saved option struct, independently of the INI. This patch:

  * skips the 1<<AntiAliasing loop so MaxMultiSamples stays 1
  * forces the accompanying allow-flag to 0
  * forces LoadVideoOptionDataFromIniFile to report AntiAliasing=0

Usage: patch_msaa.py [apply|revert|check] [--game-dir DIR]
"""
from __future__ import annotations

import argparse
import shutil
import sys
from pathlib import Path

BIN_REL = "Binaries/Linux/MN9Game.bin.x86_64"
ORIG_SUFFIX = ".orig"
BASE = 0x400000  # ELF LOAD vaddr/offset pair used by this binary

# (virtual address, original bytes, patched bytes, note)
PATCHES = [
    (0xD43989, bytes.fromhex("7e0d"), bytes.fromhex("eb0d"),
     "ApplyVideoOptionData: always skip 1<<AA loop"),
    (0xD439A1, bytes.fromhex("0f9fc0"), bytes.fromhex("31c090"),
     "ApplyVideoOptionData: force allow flag 0"),
    (0xD43491, bytes.fromhex("7424"), bytes.fromhex("eb24"),
     "LoadVideoOptionDataFromIniFile: force AntiAliasing=0"),
]


def game_dir(explicit: str | None) -> Path:
    if explicit:
        p = Path(explicit).expanduser()
    else:
        p = Path.home() / ".steam/steam/steamapps/common/Mighty No. 9"
        if not (p / BIN_REL).is_file():
            p = Path.home() / ".local/share/Steam/steamapps/common/Mighty No. 9"
    if not (p / BIN_REL).is_file():
        sys.exit(f"not a Mighty No. 9 install: {p}")
    return p


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("command", choices=("apply", "revert", "check"), default="apply", nargs="?")
    ap.add_argument("--game-dir")
    args = ap.parse_args()

    game = game_dir(args.game_dir)
    binary = game / BIN_REL
    backup = binary.with_suffix(binary.suffix + ORIG_SUFFIX)

    if args.command == "check":
        data = binary.read_bytes()
        for vaddr, orig, patched, note in PATCHES:
            off = vaddr - BASE
            cur = data[off:off + len(orig)]
            state = "patched" if cur == patched else ("original" if cur == orig else "unknown")
            print(f"{vaddr:#x} {state:8s} {note}")
        return 0

    if args.command == "revert":
        if not backup.is_file():
            sys.exit(f"no backup at {backup}")
        shutil.copy2(backup, binary)
        print(f"reverted {binary}")
        return 0

    data = bytearray(binary.read_bytes())
    for vaddr, orig, patched, note in PATCHES:
        off = vaddr - BASE
        if data[off:off + len(orig)] != orig:
            if data[off:off + len(orig)] == patched:
                print(f"already patched: {note}")
                continue
            sys.exit(f"unexpected bytes at {vaddr:#x}: {data[off:off+len(orig)].hex()}")
    if not backup.is_file():
        shutil.copy2(binary, backup)
        print(f"backup: {backup}")
    for vaddr, orig, patched, note in PATCHES:
        off = vaddr - BASE
        data[off:off + len(patched)] = patched
        print(f"patched {vaddr:#x}: {note}")
    binary.write_bytes(data)
    print("MSAA forced off")
    return 0


if __name__ == "__main__":
    sys.exit(main())

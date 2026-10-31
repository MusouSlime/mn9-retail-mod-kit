#!/usr/bin/env python3
"""Stop Mighty No. 9 from grabbing/hiding the mouse in windowed mode.

The Linux build calls SDL_SetWindowGrab(win, SDL_TRUE) and SDL_ShowCursor(0)
on focus. Patching the PLT stubs makes both no-ops:

  SDL_SetWindowGrab -> xor eax,eax; ret   (returns 0)
  SDL_ShowCursor    -> mov eax,1; ret     (returns "visible")

Usage: patch_mouse.py [apply|revert|check]
"""
from __future__ import annotations

import argparse
import sys
from pathlib import Path

BIN_REL = "Binaries/Linux/MN9Game.bin.x86_64"
BASE = 0x400000
PATCHES = [
    (0xCCFC30, bytes.fromhex("ff25f21d0c02"), bytes.fromhex("31c0c3909090"),
     "SDL_SetWindowGrab -> no-op"),
    (0xCCFEF0, bytes.fromhex("ff25921c0c02"), bytes.fromhex("b801000000c3"),
     "SDL_ShowCursor -> always visible"),
]


def game_dir() -> Path:
    p = Path.home() / ".steam/steam/steamapps/common/Mighty No. 9"
    if not (p / BIN_REL).is_file():
        p = Path.home() / ".local/share/Steam/steamapps/common/Mighty No. 9"
    if not (p / BIN_REL).is_file():
        sys.exit("game not found")
    return p


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("command", choices=("apply", "revert", "check"), nargs="?", default="apply")
    args = ap.parse_args()
    binary = game_dir() / BIN_REL
    data = bytearray(binary.read_bytes())

    if args.command == "check":
        for vaddr, orig, patched, note in PATCHES:
            off = vaddr - BASE
            cur = bytes(data[off:off + len(orig)])
            print(f"{vaddr:#x} {'patched' if cur == patched else 'original' if cur == orig else 'unknown'} {note}")
        return 0

    if args.command == "revert":
        for vaddr, orig, patched, note in PATCHES:
            off = vaddr - BASE
            if bytes(data[off:off + len(orig)]) == patched:
                data[off:off + len(orig)] = orig
        binary.write_bytes(data)
        print("reverted mouse patches")
        return 0

    for vaddr, orig, patched, note in PATCHES:
        off = vaddr - BASE
        cur = bytes(data[off:off + len(orig)])
        if cur == patched:
            print(f"already patched: {note}")
            continue
        if cur != orig:
            sys.exit(f"unexpected bytes at {vaddr:#x}: {cur.hex()}")
        data[off:off + len(patched)] = patched
        print(f"patched {vaddr:#x}: {note}")
    binary.write_bytes(data)
    print("mouse grab/visibility patched")
    return 0


if __name__ == "__main__":
    sys.exit(main())

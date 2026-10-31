#!/usr/bin/env python3
"""Patch the bundled SDL2 so the game cannot grab/hide the mouse.

The game ships its own libSDL2. Patching the function bodies (not just the
game's PLT) covers calls made from inside SDL too, including focus auto-grab.

  SDL_SetWindowGrab        -> ret
  SDL_ShowCursor           -> return 1 (visible), no state change
  SDL_SetRelativeMouseMode -> return 0, no mode change
  SDL_CaptureMouse         -> return 0
  SDL_WarpMouseInWindow    -> return 1, no warp
  SDL_WarpMouseGlobal      -> return 1, no warp

Usage: patch_sdl.py [apply|revert|check]
"""
from __future__ import annotations

import argparse
import shutil
import sys
from pathlib import Path

LIB_REL = "Binaries/Linux/lib64/libSDL2-2.0.so.0"
# (symbol, file offset, applied bytes, note)
PATCHES = [
    ("SDL_SetWindowGrab", 0x41A50, bytes.fromhex("c3"), "grab disabled"),
    ("SDL_ShowCursor", 0x40860, bytes.fromhex("b801000000c3"), "cursor stays visible"),
    ("SDL_SetRelativeMouseMode", 0x407D0, bytes.fromhex("b800000000c3"), "relative mode disabled"),
    ("SDL_CaptureMouse", 0x41C90, bytes.fromhex("b800000000c3"), "capture disabled"),
    ("SDL_WarpMouseInWindow", 0x407C0, bytes.fromhex("b801000000c3"), "no warp"),
    ("SDL_WarpMouseGlobal", 0x41C50, bytes.fromhex("b801000000c3"), "no warp"),
]


def lib_path() -> Path:
    p = Path.home() / ".steam/steam/steamapps/common/Mighty No. 9" / LIB_REL
    if not p.is_file():
        p = Path.home() / ".local/share/Steam/steamapps/common/Mighty No. 9" / LIB_REL
    if not p.is_file():
        sys.exit("libSDL2 not found")
    return p


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("command", choices=("apply", "revert", "check"), nargs="?", default="apply")
    args = ap.parse_args()
    lib = lib_path()
    backup = lib.with_suffix(lib.suffix + ".orig")
    data = bytearray(lib.read_bytes())

    if args.command == "check":
        for sym, off, patched, note in PATCHES:
            cur = bytes(data[off:off + len(patched)])
            print(f"{sym:26s} {'patched' if cur == patched else 'original'}  {note}")
        return 0

    if args.command == "revert":
        if not backup.is_file():
            sys.exit("no backup")
        shutil.copy2(backup, lib)
        print("reverted libSDL2")
        return 0

    if not backup.is_file():
        shutil.copy2(lib, backup)
        print(f"backup: {backup}")
    for sym, off, patched, note in PATCHES:
        cur = bytes(data[off:off + len(patched)])
        if cur == patched:
            print(f"already patched: {sym}")
            continue
        data[off:off + len(patched)] = patched
        print(f"patched {sym}: {note}")
    lib.write_bytes(data)
    print("SDL mouse grab disabled")
    return 0


if __name__ == "__main__":
    sys.exit(main())

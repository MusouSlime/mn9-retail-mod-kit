#!/usr/bin/env python3
"""Make the Scaleform UI fill 4:3 / 16:10 windows instead of letterboxing.

UGFxMoviePlayer::SetViewport sets up the movie viewport but leaves the
movie's scale mode at Scaleform's SM_ShowAll default, so the 16:9 UI is
letterboxed even when the engine viewport is 4:3 or 16:10.

This patch replaces the 7-byte
    movl $1, 0x64(%rax)
at 0x1a1b4ab with a call to a small stub that calls
UGFxMoviePlayer::SetViewScaleMode(this, SM_NoBorder) and then continues the
original function at 0x1a1b4b2.

SM_NoBorder fills the viewport and crops the overflowing edges.  To stretch
instead, change the `$3` immediate in gfx_scale_cave.S to `$2` (SM_ExactFit)
and rebuild gfx_scale_cave.bin.

The stub lives in the same RX-gap code cave used by Free Mouse, at
0x2b8e7b8 (Free Mouse occupies the first 332 bytes).

usage: patch_gfx_scale.py [check|apply|revert] [binary]
"""

from __future__ import annotations

import os
import struct
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
CAVE_BIN = HERE / "gfx_scale_cave.bin"

RX_VADDR = 0x400000
RX_FILESZ = 0x278E628
RX_FILESZ_NEW = 0x278E8F0
RX_MEMSZ_NEW = 0x2C000000 - RX_VADDR

CAVE_VA = 0x2B8E7B8
CAVE_OFF = CAVE_VA - RX_VADDR
SITE_VA = 0x1A1B4AB
SITE_OFF = SITE_VA - RX_VADDR
SITE_NEXT = SITE_VA + 5
ORIG = bytes.fromhex("c7406401000000")

DEFAULT = Path(
    os.environ.get(
        "MN9_BIN",
        "~/.steam/steam/steamapps/common/Mighty No. 9/"
        "Binaries/Linux/MN9Game.bin.x86_64",
    )
).expanduser()


def find_rx_phdr(data: bytes) -> int:
    phoff = struct.unpack_from("<Q", data, 0x20)[0]
    phentsize = struct.unpack_from("<H", data, 0x36)[0]
    phnum = struct.unpack_from("<H", data, 0x38)[0]
    for i in range(phnum):
        off = phoff + i * phentsize
        p_type = struct.unpack_from("<I", data, off)[0]
        p_vaddr = struct.unpack_from("<Q", data, off + 0x10)[0]
        if p_type == 1 and p_vaddr == RX_VADDR:
            return off
    raise SystemExit("RX LOAD segment not found")


def state(data: bytes) -> str:
    orig = data[SITE_OFF : SITE_OFF + 7]
    if orig == ORIG:
        return "original"
    if orig[0] == 0xE8 and orig[5:7] == b"\x90\x90":
        rel = struct.unpack_from("<i", data, SITE_OFF + 1)[0]
        if SITE_NEXT + rel == CAVE_VA:
            return "patched"
    return "mixed"


def main() -> int:
    action = sys.argv[1] if len(sys.argv) > 1 else "check"
    path = Path(sys.argv[2]).expanduser() if len(sys.argv) > 2 else DEFAULT
    data = bytearray(path.read_bytes())
    cave = CAVE_BIN.read_bytes()

    st = state(data)
    print(f"{path}: {st}")
    if action == "check":
        return 0 if st in {"patched", "original"} else 1
    if st == "mixed":
        print("refusing to touch a mixed state", file=sys.stderr)
        return 1
    if action == "apply":
        if st == "patched":
            print("already patched")
            return 0
        phdr = find_rx_phdr(data)
        filesz = struct.unpack_from("<Q", data, phdr + 0x20)[0]
        if filesz < RX_FILESZ_NEW:
            struct.pack_into("<Q", data, phdr + 0x20, RX_FILESZ_NEW)
            struct.pack_into("<Q", data, phdr + 0x28, RX_MEMSZ_NEW)
        if len(cave) > 0x2C8 - (CAVE_VA - (RX_VADDR + RX_FILESZ)):
            sys.exit("cave too large")
        data[CAVE_OFF : CAVE_OFF + len(cave)] = cave
        rel = CAVE_VA - SITE_NEXT
        data[SITE_OFF : SITE_OFF + 7] = b"\xE8" + struct.pack("<i", rel) + b"\x90\x90"
    elif action == "revert":
        if st == "original":
            print("already original")
            return 0
        data[SITE_OFF : SITE_OFF + 7] = ORIG
        data[CAVE_OFF : CAVE_OFF + len(cave)] = b"\x00" * len(cave)
    else:
        print(f"unknown action {action!r}", file=sys.stderr)
        return 2

    mode = path.stat().st_mode
    path.write_bytes(data)
    os.chmod(path, mode)
    print(f"{action}: wrote {path} ({len(data)} bytes)")
    return 0


if __name__ == "__main__":
    sys.exit(main())

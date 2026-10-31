#!/usr/bin/env python3
"""Force UE3's MSAA G-buffer resolve path to run on PC-OpenGL.

FSceneRenderTargets::ResolveGBufferSurfaces has:
    cmp GRHIShaderPlatform, 4
    jne skip
which skips G-buffer resolves for anything except platform 4 (D3D11 SM5).
PC-OpenGL is a different enum, so MSAA G-buffers are never resolved and the
screen goes white when MSAA is enabled.

This NOPs the conditional jump at 0x120b5b7 so the resolve blocks always run.

Usage: patch_resolve.py [apply|revert|check]
"""
from __future__ import annotations

import argparse
import shutil
import sys
from pathlib import Path

BIN_REL = "Binaries/Linux/MN9Game.bin.x86_64"
BASE = 0x400000
VADDR = 0x120B5B7
ORIG = bytes.fromhex("0f85ef000000")   # jne 0x120b6ac
PATCH = bytes.fromhex("909090909090")  # six NOPs


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
    backup = binary.with_suffix(binary.suffix + ".orig")
    data = binary.read_bytes()
    off = VADDR - BASE
    cur = data[off:off + len(ORIG)]

    if args.command == "check":
        print("patched" if cur == PATCH else ("original" if cur == ORIG else f"unknown {cur.hex()}"))
        return 0

    if args.command == "revert":
        if not backup.is_file():
            sys.exit("no backup")
        shutil.copy2(backup, binary)
        print("reverted")
        return 0

    if cur == PATCH:
        print("already patched")
        return 0
    if cur != ORIG:
        sys.exit(f"unexpected bytes: {cur.hex()}")
    if not backup.is_file():
        shutil.copy2(binary, backup)
        print(f"backup: {backup}")
    buf = bytearray(data)
    buf[off:off + len(PATCH)] = PATCH
    binary.write_bytes(buf)
    print("GBuffer MSAA resolve enabled on OpenGL")
    return 0


if __name__ == "__main__":
    sys.exit(main())

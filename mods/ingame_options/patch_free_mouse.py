#!/usr/bin/env python3
"""Install the Free Mouse ExternalInterface handler in the game ELF.

The game routes AS `flash.external.ExternalInterface.call(name, ...)` to
`FGFxExternalInterface::Callback`, which resolves `name` with
`UObject::FindFunction`.  This patch replaces the FindFunction call at
0x1b24387 with a call into a code cave that handles two private names:

    Mn9FreeMouseOn   -> SDL_SetWindowGrab(g_sdlWindow, 0); SDL_ShowCursor(1)
    Mn9FreeMouseOff  -> SDL_SetWindowGrab(g_sdlWindow, 1); SDL_ShowCursor(0)

All other names fall through to the original FindFunction, so normal UI
callbacks (PlaySECursor, ApplyNewOtherOptionData, ...) are unaffected.

The cave is placed in the 0x2c8-byte zero gap between the RX and RW LOAD
segments.  The RX segment is extended to cover it; no bytes are shifted.

usage: patch_free_mouse.py [check|apply|revert] [binary]
"""

from __future__ import annotations

import os
import struct
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
CAVE_BIN = HERE / "free_mouse_cave.bin"

CAVE_VA = 0x2B8E628
CAVE_OFF = CAVE_VA - 0x400000
CAVE_MAX = 0x2C8

CALL_VA = 0x1B24387
CALL_OFF = CALL_VA - 0x400000
CALL_NEXT = CALL_VA + 5

RX_VADDR = 0x400000
RX_FILESZ = 0x278E628
RX_FILESZ_NEW = 0x278E8F0
RX_MEMSZ_NEW = 0x2C000000 - RX_VADDR

DEFAULT = Path(
    os.environ.get(
        "MN9_BIN",
        "~/.steam/steam/steamapps/common/Mighty No. 9/"
        "Binaries/Linux/MN9Game.bin.x86_64",
    )
).expanduser()


def find_rx_phdr(data: bytes) -> tuple[int, int]:
    if data[:4] != b"\x7fELF":
        raise SystemExit("not an ELF file")
    phoff = struct.unpack_from("<Q", data, 0x20)[0]
    phentsize = struct.unpack_from("<H", data, 0x36)[0]
    phnum = struct.unpack_from("<H", data, 0x38)[0]
    for i in range(phnum):
        off = phoff + i * phentsize
        p_type, p_flags = struct.unpack_from("<II", data, off)
        p_vaddr = struct.unpack_from("<Q", data, off + 0x10)[0]
        if p_type == 1 and p_vaddr == RX_VADDR:
            return off, p_flags
    raise SystemExit("RX LOAD segment not found")


def state(data: bytes) -> str:
    phdr, _ = find_rx_phdr(data)
    filesz = struct.unpack_from("<Q", data, phdr + 0x20)[0]
    call = data[CALL_OFF : CALL_OFF + 5]
    if call[0] != 0xE8:
        return "mixed"
    rel = struct.unpack_from("<i", call, 1)[0]
    if filesz == RX_FILESZ_NEW and rel == CAVE_VA - CALL_NEXT:
        return "patched"
    if filesz == RX_FILESZ and rel == 0xDD9BF0 - CALL_NEXT:
        return "original"
    return "mixed"


def main() -> int:
    action = sys.argv[1] if len(sys.argv) > 1 else "check"
    path = Path(sys.argv[2]).expanduser() if len(sys.argv) > 2 else DEFAULT
    data = bytearray(path.read_bytes())
    cave = CAVE_BIN.read_bytes()
    if len(cave) > CAVE_MAX:
        raise SystemExit(f"cave too large: {len(cave)} > {CAVE_MAX}")

    phdr, _ = find_rx_phdr(data)
    st = state(data)
    print(f"{path}: {st}")

    if action == "check":
        return 0 if st in {"patched", "original"} else 1
    if st == "mixed":
        print("refusing to touch a mixed state; restore from backup", file=sys.stderr)
        return 1

    if action == "apply":
        if st == "patched":
            print("already patched")
            return 0
        struct.pack_into("<Q", data, phdr + 0x20, RX_FILESZ_NEW)
        struct.pack_into("<Q", data, phdr + 0x28, RX_MEMSZ_NEW)
        data[CAVE_OFF : CAVE_OFF + len(cave)] = cave
        rel = CAVE_VA - CALL_NEXT
        data[CALL_OFF : CALL_OFF + 5] = b"\xe8" + struct.pack("<i", rel)
    elif action == "revert":
        if st == "original":
            print("already original")
            return 0
        struct.pack_into("<Q", data, phdr + 0x20, RX_FILESZ)
        struct.pack_into("<Q", data, phdr + 0x28, RX_FILESZ)
        data[CAVE_OFF : CAVE_OFF + len(cave)] = b"\x00" * len(cave)
        data[CALL_OFF : CALL_OFF + 5] = b"\xe8" + struct.pack(
            "<i", 0xDD9BF0 - CALL_NEXT
        )
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

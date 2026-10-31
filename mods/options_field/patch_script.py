#!/usr/bin/env python3
"""Insert a bytecode block into a cooked UFunction export.

The function serial data is: <48-byte header> <script> <tail>.  The script is
DataScriptSize bytes at export+48.  This tool inserts raw bytecode at a
script-relative offset, updates ByteScriptSize/DataScriptSize and the export
size, and shifts every later export serial offset by the delta.

usage: patch_script.py <package> <exports.jsonl> <function-name> <insert-offset> <block.bin> <out>
"""

from __future__ import annotations

import argparse
import json
import struct
import sys
from pathlib import Path

HEADER = 48
BYTE_SCRIPT_SIZE = 0x28
DATA_SCRIPT_SIZE = 0x2C


def u32(b: bytes | bytearray, off: int) -> int:
    return struct.unpack_from("<I", b, off)[0]


def put_u32(b: bytearray, off: int, v: int) -> None:
    struct.pack_into("<I", b, off, v & 0xFFFFFFFF)


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("package")
    ap.add_argument("exports")
    ap.add_argument("function")
    ap.add_argument("insert_offset", type=lambda s: int(s, 0))
    ap.add_argument("block")
    ap.add_argument("output")
    args = ap.parse_args()

    data = bytearray(Path(args.package).read_bytes())
    rows = [json.loads(l) for l in Path(args.exports).read_text().splitlines() if l.strip()]
    exports = [x for x in rows if x.get("type") != "summary"]
    fn = next(x for x in exports if x["name"] == args.function)

    so, ss = fn["serial_offset"], fn["serial_size"]
    old_data = data[so : so + ss]
    old_bsize = u32(old_data, BYTE_SCRIPT_SIZE)
    old_dsize = u32(old_data, DATA_SCRIPT_SIZE)
    old_script = old_data[HEADER : HEADER + old_dsize]
    if len(old_script) != old_dsize:
        sys.exit("short script")
    tail = old_data[HEADER + old_dsize :]

    block = Path(args.block).read_bytes()
    if args.insert_offset < 0 or args.insert_offset > len(old_script):
        sys.exit("insert offset out of range")
    new_script = old_script[: args.insert_offset] + block + old_script[args.insert_offset :]
    delta = len(new_script) - len(old_script)
    if delta == 0:
        sys.exit("empty insertion")

    new_header = bytearray(old_data[:HEADER])
    put_u32(new_header, BYTE_SCRIPT_SIZE, old_bsize + delta)
    put_u32(new_header, DATA_SCRIPT_SIZE, len(new_script))
    new_export = bytes(new_header) + new_script + tail
    put_u32(data, fn["ss_field"], ss + delta)

    for e in exports:
        if e["serial_offset"] > so:
            put_u32(data, e["so_field"], e["serial_offset"] + delta)

    out = bytearray(data[:so])
    out += new_export
    out += data[so + ss :]
    Path(args.output).write_bytes(out)
    print(f"{args.function}: script {old_dsize} -> {len(new_script)} (+{delta}), export {ss} -> {len(new_export)}")
    print(f"wrote {args.output} ({len(out)} bytes)")
    return 0


if __name__ == "__main__":
    sys.exit(main())

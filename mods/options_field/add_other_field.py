#!/usr/bin/env python3
"""Add a bool field to TOtherOptionData in a cooked MN9Game.upk.

The struct is 16 bytes in memory:

    Language 0, VoiceLanguage 4, bDecideCancelButtonNormal 8,
    padding 9-11, RestNum 12

The new bool is linked between bDecideCancelButtonNormal and RestNum, so it
lands at offset 9 (inside the padding), RestNum stays at 12, and the struct
size stays 16.  The native UFunction ABI is therefore unchanged.

This tool:
  * clones the bDecideCancelButtonNormal UBoolProperty export entry
  * links it after bDecideCancelButtonNormal by patching NextField
  * appends the new property serial data at EOF
  * inserts the export table entry and shifts every later serial offset

usage: add_other_field.py <in.upk> <exports.jsonl> <out.upk> [name-index]

Default name index is SoundTestAC (25497), an existing FName in this package.
The AS-facing field name is independent and chosen in the GFX bytecode.
"""

from __future__ import annotations

import argparse
import json
import struct
import sys
from pathlib import Path

ENTRY_SIZE = 68
SUMMARY_EXPORT_COUNT = 0x21
SUMMARY_EXPORT_OFFSET = 0x25
SUMMARY_HEADER_SIZE = 0x08
SUMMARY_DEPENDS_OFFSET = 0x31
SUMMARY_GUIDS_OFFSET = 0x35

# Export entry field offsets
E_CLASS = 0x00
E_OUTER = 0x08
E_NAME = 0x0C
E_NAME_NUM = 0x10
E_SIZE = 0x20
E_OFFSET = 0x24

# UProperty serialized UField/NextField offset
PROP_NEXT = 0x0C


def u32(b: bytes | bytearray, off: int) -> int:
    return struct.unpack_from("<I", b, off)[0]


def put_u32(b: bytearray, off: int, v: int) -> None:
    struct.pack_into("<I", b, off, v & 0xFFFFFFFF)


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("package")
    ap.add_argument("exports")
    ap.add_argument("output")
    ap.add_argument("name_index", nargs="?", type=int, default=25497)
    args = ap.parse_args()

    data = bytearray(Path(args.package).read_bytes())
    rows = [json.loads(l) for l in Path(args.exports).read_text().splitlines() if l.strip()]
    summary = next(x for x in rows if x.get("type") == "summary")
    exports = [x for x in rows if x.get("type") != "summary"]

    base = next(x for x in exports if x["name"] == "bDecideCancelButtonNormal")
    rest = next(x for x in exports if x["name"] == "RestNum" and x["serial_size"] == 40)
    lang = next(x for x in exports if x["name"] == "Language" and x["serial_size"] == 40)

    # The existing property export indices (0-based manifest i) -> UE package index +1.
    new_i = len(exports)
    new_pkg = new_i + 1
    rest_pkg = rest["i"] + 1
    base_pkg = base["i"] + 1

    export_off = summary["export_offset"]
    export_count = len(exports)
    # Export entries are 68 or 72 bytes; the table ends at the last entry end.
    insert_at = max(e["table_offset"] + e["table_size"] for e in exports)
    old_len = len(data)

    # Clone the export table entry.
    entry = bytearray(data[base["table_offset"] : base["table_offset"] + ENTRY_SIZE])
    if len(entry) != ENTRY_SIZE:
        sys.exit("short export entry")

    # New serial data = clone of bDecideCancelButtonNormal, linked before RestNum.
    serial = bytearray(data[base["serial_offset"] : base["serial_offset"] + base["serial_size"]])
    if len(serial) != 40:
        sys.exit("unexpected property serial size")
    put_u32(serial, PROP_NEXT, rest_pkg)

    # Insert 68 bytes for the new export table entry.
    new_data = bytearray(data[:insert_at]) + bytearray(ENTRY_SIZE) + bytearray(data[insert_at:])

    # Every existing serial offset moves by 68.
    for e in exports:
        put_u32(new_data, e["so_field"], e["serial_offset"] + ENTRY_SIZE)

    # The generation table also records the export count; the object array is
    # sized from it, so it must match the summary.
    gen_off = data.find(export_count.to_bytes(4, "little"), 0x40, export_off)
    if gen_off >= 0:
        put_u32(new_data, gen_off, export_count + 1)

    # Summary offsets after the export table.
    put_u32(new_data, SUMMARY_EXPORT_COUNT, export_count + 1)
    put_u32(new_data, SUMMARY_HEADER_SIZE, u32(new_data, SUMMARY_HEADER_SIZE) + ENTRY_SIZE)
    put_u32(new_data, SUMMARY_DEPENDS_OFFSET, u32(new_data, SUMMARY_DEPENDS_OFFSET) + ENTRY_SIZE)
    put_u32(new_data, SUMMARY_GUIDS_OFFSET, u32(new_data, SUMMARY_GUIDS_OFFSET) + ENTRY_SIZE)

    # Patch bDecideCancelButtonNormal.NextField to the new property (shifted +68).
    base_next = base["serial_offset"] + ENTRY_SIZE + PROP_NEXT
    put_u32(new_data, base_next, new_pkg)

    # Build the new entry.
    put_u32(entry, E_NAME, args.name_index)
    put_u32(entry, E_NAME_NUM, 1)
    put_u32(entry, E_SIZE, 40)
    new_serial_off = len(new_data)  # append at EOF
    put_u32(entry, E_OFFSET, new_serial_off)
    new_data[insert_at : insert_at + ENTRY_SIZE] = entry

    # Append the new property payload.
    new_data += serial

    Path(args.output).write_bytes(new_data)
    print(f"new export i={new_i} pkg={new_pkg} name_index={args.name_index}")
    print(f"inserted entry at {insert_at}; serial at {new_serial_off}")
    print(f"{old_len} -> {len(new_data)} bytes")
    print(f"linked {base['name']}({base_pkg}) -> new({new_pkg}) -> {rest['name']}({rest_pkg})")
    return 0


if __name__ == "__main__":
    sys.exit(main())

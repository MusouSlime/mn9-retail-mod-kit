#!/usr/bin/env python3
"""Replace a SwfMovie (UGFxRawData) payload in an uncompressed UE3 package.

The export data is laid out as:

    <header ...> <int32 RawData array size> "GFX\\x0f" <gfx bytes> <trailing props>

`RawData array size` is the inner size + 4 and sits at `magic - 12`; the inner
GFX size sits at `magic - 4`.  A larger replacement shifts every later export,
so this rewrites the export table offsets/sizes as well.

usage: pack_swf.py <package> <exports.jsonl> <gfx> <out> [export-name]
"""

from __future__ import annotations

import argparse
import json
import struct
import sys
from pathlib import Path


def i32(b: bytearray, off: int) -> int:
    return struct.unpack_from("<i", b, off)[0]


def put_i32(b: bytearray, off: int, v: int) -> None:
    struct.pack_into("<i", b, off, v)


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("package")
    ap.add_argument("exports")
    ap.add_argument("gfx")
    ap.add_argument("output")
    ap.add_argument("name", nargs="?", default="Option")
    args = ap.parse_args()

    data = bytearray(Path(args.package).read_bytes())
    lines = [json.loads(l) for l in Path(args.exports).read_text().splitlines() if l.strip()]
    summary = next(x for x in lines if x.get("type") == "summary")
    exports = [x for x in lines if x.get("type") != "summary"]

    candidates = [e for e in exports if e["name"] == args.name]
    if not candidates:
        sys.exit(f"{args.name} export not found")
    # The texture/class-default exports can share the name; the movie is the
    # largest one with a GFX magic.
    opt = None
    for e in sorted(candidates, key=lambda x: -x["serial_size"]):
        so, ss = e["serial_offset"], e["serial_size"]
        if ss <= 0 or so < 0:
            continue
        if b"GFX\x0f" in data[so : so + ss][:2048]:
            opt = e
            break
    if opt is None:
        sys.exit(f"{args.name}: no export with a GFX payload")

    so, ss = opt["serial_offset"], opt["serial_size"]
    magic = data.find(b"GFX", so, so + ss)
    if magic < 0:
        sys.exit("GFX payload not found")

    old_size = i32(data, magic + 4)
    if old_size <= 0 or magic + 8 + old_size > len(data):
        sys.exit(f"bad GFX size {old_size}")
    if i32(data, magic - 4) != old_size:
        sys.exit("inner GFX size field does not match")
    if i32(data, magic - 12) != old_size + 4:
        sys.exit("RawData array size field does not match")

    new = Path(args.gfx).read_bytes()
    if new[:4] != b"GFX\x0f":
        sys.exit("replacement is not a GFX file")
    new_size = len(new)
    delta = new_size - old_size

    insert_at = magic + old_size
    out = bytearray(data[:magic])
    out += new
    out += data[insert_at:]

    put_i32(out, magic - 12, new_size + 4)
    put_i32(out, magic - 4, new_size)
    put_i32(out, opt["ss_field"], ss + delta)

    shifted = 0
    for e in exports:
        if e["serial_offset"] >= insert_at:
            put_i32(out, e["so_field"], e["serial_offset"] + delta)
            shifted += 1

    for key in ("depends_offset", "import_export_guids_offset", "thumbnail_table_offset"):
        v = summary.get(key, 0)
        if v and v >= insert_at:
            print(f"warning: {key} is after payload ({v}); check manually", file=sys.stderr)

    Path(args.output).write_bytes(out)
    print(f"{args.name}: {old_size} -> {new_size} bytes (delta {delta}), {shifted} exports shifted")
    print(f"wrote {args.output} ({len(out)} bytes)")
    return 0


if __name__ == "__main__":
    sys.exit(main())

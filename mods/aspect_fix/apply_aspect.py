#!/usr/bin/env python3
"""Set the MN9 render aspect and optionally fill the Scaleform UI.

usage:
  apply_aspect.py --game-dir <dir> --width 1280 --height 960
  apply_aspect.py --game-dir <dir> --width 1920 --height 1080 --no-fill
  apply_aspect.py --game-dir <dir> --revert
"""

from __future__ import annotations

import argparse
import math
import struct
import subprocess
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent

KNOWN_ASPECTS = [
    1.7777801,
    1.7777778,
    1.3333334,
    1.6,
]


def find_float32(data: bytes, value: float) -> list[int]:
    pat = struct.pack("<f", value)
    return [i for i in range(len(data) - 3) if data.startswith(pat, i)]


# The Default__MN9GamePlayerCamera export sits before the Option GFX payload,
# so this offset is stable across our GFX repacks.  Fall back to scanning for
# the retail 16:9 value if the package was restored/re-cooked.
DEFAULT_ASPECT_OFFSET = 7922322


def patch_aspect(game_dir: Path, aspect: float) -> str:
    upk = game_dir / "MN9Game" / "CookedLinux" / "MN9Game.upk"
    if not upk.is_file():
        return f"missing {upk}"
    data = bytearray(upk.read_bytes())

    off = -1
    if DEFAULT_ASPECT_OFFSET + 4 <= len(data):
        value = struct.unpack_from("<f", data, DEFAULT_ASPECT_OFFSET)[0]
        if 0.5 < value < 3.0:
            off = DEFAULT_ASPECT_OFFSET

    if off < 0:
        hits: list[int] = []
        for value in KNOWN_ASPECTS:
            hits = find_float32(bytes(data), value)
            if len(hits) == 1:
                break
            hits = []
        if not hits:
            return "DefaultAspectRatio value not found (was the package restored?)"
        if len(hits) > 1:
            return f"ambiguous: {len(hits)} DefaultAspectRatio candidates"
        off = hits[0]
    old = struct.unpack_from("<f", data, off)[0]
    struct.pack_into("<f", data, off, aspect)
    upk.write_bytes(data)
    return f"DefaultAspectRatio {old:.6f} -> {aspect:.6f} at 0x{off:x}"


def run_patch(action: str) -> str:
    script = HERE / "patch_movie_scale.py"
    out = subprocess.run(
        [sys.executable, str(script), action],
        capture_output=True,
        text=True,
    )
    return (out.stdout + out.stderr).strip()


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--game-dir", required=True)
    ap.add_argument("--width", type=int)
    ap.add_argument("--height", type=int)
    ap.add_argument("--fill", dest="fill", action="store_true", default=None)
    ap.add_argument("--no-fill", dest="fill", action="store_false")
    ap.add_argument("--revert", action="store_true")
    args = ap.parse_args()

    game = Path(args.game_dir).expanduser()
    if args.revert:
        print(run_patch("revert"))
        print("aspect reverted to 16:9")
        print(patch_aspect(game, 16 / 9))
        return 0

    if not args.width or not args.height:
        print("--width and --height are required", file=sys.stderr)
        return 2

    aspect = args.width / args.height
    print(patch_aspect(game, aspect))

    want_fill = args.fill if args.fill is not None else abs(aspect - 16 / 9) > 0.01
    if want_fill:
        print(run_patch("apply"))
    else:
        print(run_patch("revert"))
    return 0


if __name__ == "__main__":
    sys.exit(main())

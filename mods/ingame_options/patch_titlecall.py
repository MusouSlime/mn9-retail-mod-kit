#!/usr/bin/env python3
"""Make the title menu skip AddDLCTitleCall().

`MN9UITitleMenu.LoadDLC()` runs:

    CheckDLCGoldenHero();
    CheckDLCRetroHero();
    AddDLCTitleCall();

`AddDLCTitleCall()` walks DLCTitleCallPl03..Pl07 and calls `LoadPackageFile`
for each.  On a retail install that only ships Pl03, loading Pl06 hard-crashes
the game about 20 seconds after boot (SIGSEGV in ULinkerLoad::CreateExport).

The three calls are plain no-argument virtual calls in the cooked bytecode.
This patch rewrites the 4-byte name index of the third call from
`AddDLCTitleCall` (456) to the empty function `CheckDLCRay` (8010).  The call
becomes a no-op, the bytecode length and every export offset stay unchanged,
and `AddDLCTitleCall` keeps its original body for reference.

usage: patch_titlecall.py [check|apply|revert] [package.upk]
"""

from __future__ import annotations

import os
import sys
from pathlib import Path

ORIG = bytes.fromhex("1b c8 01 00 00 00 00 00 00 16 04 0b 53")
PATCHED = bytes.fromhex("1b 4a 1f 00 00 00 00 00 00 16 04 0b 53")

DEFAULT = Path(
    os.environ.get(
        "MN9_UPK",
        "~/.steam/steam/steamapps/common/Mighty No. 9/"
        "MN9Game/CookedLinux/MN9Game.upk",
    )
).expanduser()


def main() -> int:
    action = sys.argv[1] if len(sys.argv) > 1 else "check"
    pkg = Path(sys.argv[2]).expanduser() if len(sys.argv) > 2 else DEFAULT
    data = bytearray(pkg.read_bytes())

    n_orig, n_patched = data.count(ORIG), data.count(PATCHED)
    if n_orig == 1 and n_patched == 0:
        state = "original"
    elif n_orig == 0 and n_patched == 1:
        state = "patched"
    else:
        print(f"unexpected state: original={n_orig} patched={n_patched}", file=sys.stderr)
        return 1

    print(f"{pkg}: {state} (LoadDLC -> AddDLCTitleCall)")

    if action == "check":
        return 0
    if action == "apply":
        if state == "patched":
            print("already patched")
            return 0
        i = data.find(ORIG)
        data[i : i + len(ORIG)] = PATCHED
    elif action == "revert":
        if state == "original":
            print("already original")
            return 0
        i = data.find(PATCHED)
        data[i : i + len(PATCHED)] = ORIG
    else:
        print(f"unknown action {action!r}", file=sys.stderr)
        return 2

    mode = pkg.stat().st_mode
    pkg.write_bytes(data)
    os.chmod(pkg, mode)
    print(f"{action}: wrote {pkg} ({len(data)} bytes)")
    return 0


if __name__ == "__main__":
    sys.exit(main())

# MSAA resolve (OpenGL) — not usable

`FSceneRenderTargets::ResolveGBufferSurfaces` skips the G-buffer resolve for
anything that is not platform 4 (D3D11 SM5). On PC-OpenGL the resolve never
runs, which is why MSAA renders incorrectly.

`patch_resolve.py` NOPs the platform check at `0x120b5b7`:

```
0f 85 ef 00 00 00   jne  ...   ->   90 90 90 90 90 90
```

## Result

With `MaxMultiSamples=4` + `bAllowD3D9MSAA=True` the patched build reaches the
stage load and **segfaults after ~7 s** (before the first gameplay frame).
The crash is in the renderer during the multisample setup/resolve, not in the
game logic.

**Do not ship this patch.** It is reverted; the binary is back to the original
six bytes and the game runs with `MaxMultiSamples=1` + MLAA.

Revert manually (do not use `patch_resolve.py revert`; it copies a `.orig`
that may predate the Free Mouse / MovieImpl caves):

```sh
python3 - <<'PY'
from pathlib import Path
p = Path.home()/".steam/steam/steamapps/common/Mighty No. 9/Binaries/Linux/MN9Game.bin.x86_64"
b = bytearray(p.read_bytes()); off = 0x120b5b7 - 0x400000
b[off:off+6] = bytes.fromhex("0f85ef000000")
p.write_bytes(b)
PY
```

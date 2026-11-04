# Bundled ReXGlue runtime

These payloads are copied into a managed port by Xel Launcher's
**Install the port from XBLA** action. They contain the recompiled host
executable and its runtime libraries, not game data.

- `linux-x64/`: `mn9`, `librexruntime.so`, `librexgpu-xenos.so`, `extract_stfs`
  (Linux build, stripped).
- `win-x64/`: `mn9.exe`, `rexruntime.dll`, `rexgpu-xenos.dll`,
  `extract_stfs.exe`, plus the Microsoft VC++ runtime DLLs.

Target: Mighty No. 9 XBLA, Title ID `584114A4`, XEX version `0.0.0.4`.
The XBLA package itself is supplied by the user; it is extracted with the
bundled `extract_stfs`.

The Windows payload was cross-built from the same generated project with
clang-cl/LLD in MSVC mode against the ReXGlue SDK 0.10.0 `win-amd64` package,
using the MSVC 14.51 / Windows SDK 10.0.26100 headers and libraries. The
`roundevenf` shim in `src/main.cpp` covers a UCRT gap in that toolchain.

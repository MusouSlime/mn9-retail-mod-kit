# Mighty No. 9 retail mod kit

Patcher and reversible mods for the retail PC build of **Mighty No. 9**
(Steam app 314710). No game code, ROMs, or data packages are included.

- **Windows:** download `Mn9Patcher-win64.zip` from **Releases**, extract, run
  `Mn9Patcher.exe`. Self-contained, no .NET install needed.
- **Linux:** source build or the `XelLauncher-x86_64.AppImage` release asset;
  the low-resource profiles and ELF mods also run there.

## Xel Launcher 1.0.1

The first stable release of the cross-platform launcher, built around the
retail PC build and the ReXGlue Xbox 360 port.

- **PC patcher** — resolution, frame cap, texture pool, physics substeps,
  difficulty (Normal/Hard/Hyper/Maniac), effects, Large Address Aware,
  config locking and restore.
- **Port tab (Linux)** — ReXGlue backend, quality presets, start mode,
  DLC/license mask, user-data root, diagnostics and keyboard binds.
- **Post-process** — Metallic (default), Noir, Dot Matrix, Cinematic,
  Posterize and Retro, plus FXAA. Applied from the first frame.
- **Audio** — replace the soundtrack from a folder (random/playlist/shuffle),
  SFX pitch, voice mute, ducking, night mode and levels.
- **Turbo auto-fire (Linux PC build)** — holding the fire button keeps firing
  at 6–20 taps per second; optional runtime toggle key, mouse fire included.
- **Character select** — Beck, Call, Ray, Gold and Retro via a spawn override
  (Linux).
- **Saves** — timestamped backups, manifests, restore and safe mode.

Release binaries bundle launcher interface art extracted from the retail game
for the header/turntables. That art is not part of the MIT-licensed source and
is not committed to this repository; a source build runs without it unless you
supply an `assets/` folder.

## Windows quick start

1. Install the retail game through Steam.
2. Download the latest patcher zip from
   [Releases](../../releases/latest) and extract it anywhere.
3. Run `Mn9Patcher.exe`, press **Detect** (or **Browse** to
   `...\steamapps\common\Mighty No. 9`).
4. Pick a profile/resolution and press **Apply settings**.
5. Launch the game normally through Steam.

The patcher writes the loose UE3 INIs and keeps a baseline in
`<game>\.mn9patcher\baseline`. **Restore original** puts them back.
**Fullscreen** automatically uses the panel's native resolution (windowed
configs are unchanged); non-native exclusive fullscreen black-screens on the
SDL2 build, so the patcher refuses to configure it.

### What the patcher changes

- Resolution, fullscreen/VSync, frame cap, texture pool, physics substeps,
  audio voices.
- Shadows, bloom/FXAA, motion blur, light shafts, distortion, lens flares, AO,
  MLAA, MSAA (MSAA is intentionally off; see below).
- Intro/loading movies off (the `OpDemo` intro stage is untouched), Steam voice
  off.
- Locks the INIs + `SystemData.dat` read-only so the game cannot overwrite the
  profile on exit.
- Save backups: timestamped snapshots of `MN9Game\SaveData`, SHA-256 manifest,
  newest 20 kept, `before-restore` safety snapshot.

### Windows testing checklist

See [`WINDOWS-TESTING.md`](WINDOWS-TESTING.md) for the exact sequence (fresh
detect, fullscreen, potato, mouse toggle, save backup/restore, safe mode).

## Linux mods (included, source only)

| Path | What it does | Notes |
|---|---|---|
| `mods/patcher` | Xel Launcher: PC patcher, port tab, postfx/audio/turbo shims, character select | Avalonia; Windows + Linux |
| `mods/lowspec` | `low`…`lowest` and `quality`/`maximum` INI profiles | Python, no game data |
| `mods/controller_fix` | Hides the Steam Input virtual pad so only one controller is seen | Launch wrapper |
| `mods/safe_mode` | Restores a known-good config after an abnormal exit | Bash wrapper |
| `mods/aspect_fix` | 4:3 / 16:10 render aspect + Scaleform `SM_ExactFit` fill | Linux ELF offsets |
| `mods/ingame_options` | Cursor Hold row + title DLC-call fix | Scripts; rebuild the retail GFX locally |
| `mods/no_mouse_grab` | Older SDL/PLT mouse-capture patches | Linux only |
| `mods/msaa_off` | Force MSAA off in `UMN9_SystemData` | Linux ELF |
| `mods/msaa_resolve` | Research notes: forcing the OpenGL resolve segfaults | Do not ship |
| `mods/options_field` | Blocked research: adding a real `TOtherOptionData` field | Reference |

The 4:3 HUD-corner tool and the Cursor Hold GFX payload are **not** in this
repo: they embed retail-derived Scaleform payloads. They stay in the personal
build and are regenerated from a local retail install.

## Build

Requires the .NET 10 SDK.

```sh
# Windows single-file exe + zip input
mods/patcher/build_win.sh

# rebuild the Linux LD_PRELOAD shims (postfx/audio/turbo)
mods/patcher/build_mods.sh

# Linux AppImage (needs mksquashfs + appimage runtime)
mods/patcher/build_appimage.sh

# Linux folder build
mods/patcher/build_win.sh linux
```

`tools/ue3/Mn9Exports` is the package manifest dumper used by the GFX repack
scripts (`mods/ingame_options/pack_swf.py`, aspect/HUD tooling).

## Known limits

- **MSAA** stays off: the OpenGL G-buffer resolve is skipped and forcing it
  crashes. MLAA is the post-AA path.
- **SSAA > 100%** (`ScreenPercentage=150`) blackens the stage-select/menu 3D on
  this build; keep 100%.
- **Non-native exclusive fullscreen** black-screens under XWayland; the patcher
  forces the native mode.
- **120 fps** works (`UseVsync=False`, `MaxSmoothedFrameRate=120`) but a 60 Hz
  panel tears; a real 120 Hz display is required for a clean image.
- Windows binary mods (aspect fill, HUD corners, Cursor Hold) are Linux-only;
  the Windows build applies config changes and the SDL mouse patch only.

## Bonus releases

- **Mega Man 1: Beck — WIP bonus** — [IPS + notes](bonus/mm1-beck/README.md).
  A Mega Man 1 (NES) romhack, separate from the MN9 patcher.

## License

MIT. Mighty No. 9 is © 2016 Deep Silver / Comcept / Inti Creates. This project
ships no game content and is not affiliated with the rights holders.

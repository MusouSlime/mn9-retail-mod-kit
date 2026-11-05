# Mighty No. 9 retail mod kit

Patcher and reversible mods for the retail PC build of **Mighty No. 9**
(Steam app 314710). No game code, ROMs, or data packages are included.

- **Windows:** download `Mn9Patcher-win64.zip` from **Releases**, extract, run
  `Mn9Patcher.exe`. Self-contained, no .NET install needed.
- **Linux:** source build or the `XelLauncher-x86_64.AppImage` release asset;
  the low-resource profiles and ELF mods also run there.

## Xel Launcher 1.4.3

Two top-level pages — **Port** and **Steam** — with the same sub-tabs
(Setup, Display, Quality, Audio, Gameplay, Character, Saves, Advanced).
Both platforms are supported, including a Windows ReXGlue runtime.

- **Port page** — install a self-contained Xbox 360 port from the user's
  XBLA package (and optional DLC/addons). The launcher extracts STFS with the
  bundled tool, copies the bundled runtime, stages addons and writes
  `license_mask=4294967295` so the build is fully unlocked.
- **Steam page** — the retail PC patcher: resolution, frame cap, texture
  pool, physics substeps, effects, turbo auto-fire, Large Address Aware,
  config locking and restore.
- **DLC/addons** — Golden Beck, MegaXel, Ray and Retro Hero are staged into
  the port's content folder; pick the form in game.
- **In-game mod loader** — **DOWNLOAD CONTENT** in the main menu opens a
  native loader instead of the dead Xbox Live store. It lists the addon packs
  with installed/missing status and sizes, rescans the folder, and exposes
  texture detail and frame pacing. **Mods / DLC** in the launcher opens the
  port with the loader already on screen.
- **Post-process** — Metallic (default), Noir, Dot Matrix, Cinematic,
  Posterize and Retro, plus FXAA. Applied from the first frame.
- **Audio** — replace the soundtrack from a folder (random/playlist/shuffle),
  SFX pitch, voice mute, ducking, night mode and levels. MP3 uses mpg123;
  OGG/FLAC/WAV use libsndfile.
- **Turbo auto-fire (Linux PC build)** — holding the fire button keeps firing
  at 6–20 taps per second. R3 can be reassigned from Direct Change 3 to the
  auto-fire toggle; mouse fire included.
- **Character select** — Beck, Call, Ray, Gold, Retro and **MegaXel** via a
  spawn override (Linux). MegaXel does not need the Xbox 360 DLC: the Steam
  build already contains the Spw11 pawn, model and materials, and the launcher
  now spawns that form directly.
- **Saves** — timestamped backups, manifests, restore and safe mode.
- **Fast boot** — one A at the title reaches the main menu. The press is held
  for 150 ms so the 60 Hz guest cannot miss it; the controller was verified on
  the released build.
- **Texture detail (port)** — native, low (skip the largest mip) or very low
  (skip two), without repacking the game data.

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

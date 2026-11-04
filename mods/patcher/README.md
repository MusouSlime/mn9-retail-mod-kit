# Xel Launcher

Cross-platform GUI patcher for the retail **Mighty No. 9** PC build. It edits
the loose UE3 config files, backs them up, and can optionally patch the mouse
capture path. Builds as a Windows `.exe` and a Linux `.AppImage` from the same
Avalonia code.

The launcher bundles extracted game UI art and rendered character turntables
for its interface; it does not include game code or full data packages. The
Windows release is x86-64 (PE32+); it runs on 64-bit Windows 10/11.

## Build

```sh
./build_win.sh          # dist/win-x64/Mn9Patcher.exe + dist/Mn9Patcher-win64.zip
./build_appimage.sh     # dist/XelLauncher-x86_64.AppImage
./build_win.sh linux    # dist/linux-x64/Mn9Patcher (folder build)
```

Requires the .NET 10 SDK. The AppImage needs `mksquashfs` and an AppImage
runtime (`appimage/runtime-x86_64`).

## Use

Run `Mn9Patcher.exe` (Windows) or `XelLauncher-x86_64.AppImage` (Linux).

- The game folder is detected from Steam; **Browse** lets you pick it manually.
- Tick the settings you want.
- Press **Apply settings** (or Ctrl+Enter).
- **Restore original** copies the pre-patch backup back and reverts the mouse
  patch.
- **Launch with Safe Mode** restores a known-good config if the previous exit
  was abnormal, then starts the game and watches it.
- **Character select** writes `.mn9charsel/character` and, on Linux,
  **Launch with character** starts the game through the spawn-override
  wrapper. The AppImage bundles `mn9-charselect.sh` and `libmn9spawn.so`
  under `usr/spawn_override/`, so the packaged build works without the
  source tree.

## ReXGlue port

The launcher has two top-level pages, **Port** and **Steam**. Both use the
same sub-tabs: **Setup**, **Display**, **Quality**, **Audio**, **Gameplay**,
**Character**, **Saves** and **Advanced**, so the same workflow applies to
either backend.

The **Port** page can build a self-contained Xbox 360 port from the user's own
XBLA package:

1. Pick the XBLA package file (or an already-extracted `default.xex` folder).
2. Optionally add DLC/addon packages.
3. Pick an install folder and press **Extract && install port**.

The launcher remembers the real managed-port location in
`~/.config/xel-launcher/last-port.txt` (Windows:
`%APPDATA%\xel-launcher\last-port.txt`). If the install-folder field is
changed to an empty or moved path, launch/apply falls back to the remembered
port and then scans common install roots for a valid `port.conf` +
`game/default.xex`.

A development port is also recognized without `port.conf` when it contains
`game/default.xex` plus `run-mn9.sh`, `run-mn9.ps1`, or the built
`out/build/<platform>/mn9` executable. The Windows helper is `run-mn9.ps1`.

The launcher extracts the STFS package with the bundled `extract_stfs`, copies
the bundled ReXGlue runtime, installs addons into
`game/Content/.../584114A4/00000002`, and writes `port.conf` with
`license_mask=4294967295` so the build is fully unlocked. The runtime payloads
live in `port-runtime/linux-x64` and `port-runtime/win-x64`; the AppImage and
Windows build copy the matching one next to the launcher. No game data or
runtime is bundled inside the installer itself.

The Port page also covers display, quality preset and fine overrides, frame rate
(60 FPS default, or 30 FPS low-power), FXAA, low-memory mode, keyboard-as-pad,
intro skip timing, port audio gain/mute/night mode, addon/DLC staging, saves,
shader cache, logs and keybindings. Intro skip
works for managed ports on Linux (xdotool) and Windows (Win32 key events).

The Steam page contains the retail PC build's display, effects, audio,
gameplay, character/palette, save-backup and advanced options. The global
Game box and Apply/Launch bar stay available above/below both pages.

The four known addon packs (Golden Beck, MegaXel, Ray, Retro Hero) are staged
into the port's content folder; select the form in the game's character/Extra
menu. The launcher does not force Beck's controller onto those packs.

PC-only settings remain on the PC tabs: per-effect rendering toggles, custom
post styles, per-bus audio/pitch/dialog shuffle/music replacement, cutscene
auto-skip/disable-dialog, the boss-palette shim, and the UE3 frame cap/texture
pool/physics options. They depend on PC UE3, OpenGL or OpenAL and are labelled
as PC-only on the Port tab.

## Look and feel

The launcher uses assets extracted from the retail game:

- Header background: `TitleMenu.upk` title background.
- Logo and Xel mark: `TitleMenu.upk` title art.
- Character portraits and panel art: `FileSelect` and stage-select textures.
- Character tab: animated 3D turntables rendered from the game's own
  `Spw09`, `Spw10`, `PL02`, `PL03` and `RetroBeck` skeletal meshes, with
  their body/head/eye/mouth textures.

The rendered turntables are in `assets/models/*_sheet.png` (4x4 sprite
sheets, 16 frames each). The launcher animates them in the Character tab.

Settings:

| Checkbox | Effect |
|---|---|
| Shadows | Dynamic + light-environment shadows |
| Bloom / FXAA | Bloom and the game's FXAA path |
| Motion blur | Motion blur + pause blur |
| Light shafts | Volumetric light shafts |
| Distortion | Distortion + radial blur |
| Lens flares | Lens flares |
| Ambient occlusion | AO |
| MLAA (post AA) | Post-process MLAA |
| MSAA | Off by default — this build's MSAA path is broken |
| Fullscreen | Screen type — forces the panel's native resolution (non-native exclusive fullscreen black-screens on SDL2/XWayland) |
| VSync | Vsync |
| Steam voice | Enables Steam voice (off = no voice errors) |
| Play intro movies | Off = skip startup/loading movies; the `OpDemo` intro stage is untouched |
| Free the mouse (external patch) | Off by default while the in-game row is prototyped |
| Large Address Aware | Sets the LAA flag on the 32-bit game exe so it can address up to 4 GB on 64-bit Windows (reversible; does not make it 64-bit) |
| Lock config | Lock INIs + `SystemData.dat` so the game cannot save settings |

Numeric/choice fields: resolution (16:9, 16:10 and 4:3 presets — 640x360,
960x540, 1024x768, 1280x720, 1280x800, 1280x960, 1440x1080, 1680x1050,
1920x1080, 1920x1200), frame cap (30/60/62/72/90/120/144/240),
texture pool (64/128/256/384/512/768/1024 MB) and physics substeps
(1/2/3/4/5/6/8/10). Each dropdown is labelled with its trade-off; 62 FPS,
256 MB and 5 substeps are the game defaults.

The game keeps `AspectRatioAxisConstraint=AspectRatio_MaintainXFOV`, so
narrower screens add vertical view instead of stretching. 4:3 and 16:10 are
included in the in-game Screen Size list via the engine resolution list.

For 4:3 / 5:4 presets the patcher also sets
`Default__MN9GamePlayerCamera.DefaultAspectRatio` in `MN9Game.upk`. On Linux it
turns on the Scaleform `SM_ExactFit` fill, so the 16:9 UI fills the window
instead of letterboxing. On Windows the 3D aspect is set, but the GFx fill
cave is ELF-only for now, so the UI can still letterbox. Picking a 16:9 preset
reverts both. 16:10 is left un-filled: the engine clamps the wide bucket to
16:9 before GFx runs, so a 1280x800 window still shows 40 px bars. See
`mods/aspect_fix/README.md`.

## 32-bit game (Large Address Aware)

The retail Windows `MN9Game.exe` is a 32-bit UE3 build, so Task Manager will
always show it as 32-bit. It cannot be converted to 64-bit without rebuilding
the engine/game from source. **Large Address Aware** sets the PE header flag so
the 32-bit process can address up to 4 GB instead of 2 GB on 64-bit Windows —
useful for high-resolution textures and 4K shadows. It writes
`MN9Game.exe.laa.orig` once and Restore puts the original exe back.

The **Linux** build (`MN9Game.bin.x86_64`) is the 64-bit one.

## Mouse fix

The external mouse patch is **off by default** now that the in-game Other row is
being prototyped. Enable it only if you want the pre-launch binary patch:

- **Linux:** patches the game's `SDL_SetWindowGrab`/`SDL_ShowCursor` PLT stubs
  and the function bodies in the bundled `libSDL2-2.0.so.0` (grab, show, relative
  mode, capture, warp). Backups are `.orig` files; Restore reverts them.
- **Windows:** patches the same exports inside the game's `SDL2.dll`
  (`Binaries/Win64`, `Win32`, the game root, or anywhere else under the
  install — the patcher searches recursively), with a `.orig` backup.

Disabling the mouse patch restores only those function bytes, so unrelated
binary edits (MSAA, Free Mouse, the aspect fill cave) are left alone.

## Save backups

The patcher keeps timestamped snapshots of `MN9Game/SaveData`:

- **Backup saves** copies every file in `SaveData` to
  `<game>/.mn9patcher/saves/<timestamp>_manual/` and writes a `manifest.json`
  with sizes and SHA-256 hashes.
- **Restore selected** restores that snapshot. It first takes a
  `before-restore` safety snapshot of the current saves, preserves the
  destination file mode (a locked `SystemData.dat` stays `0444`/ReadOnly), and
  refuses to run while the game is open.
- The newest 20 snapshots are kept.

CLI:

```sh
Mn9Patcher --cli --game-dir "..." --backup-saves
Mn9Patcher --cli --game-dir "..." --list-saves
Mn9Patcher --cli --game-dir "..." --restore-saves --save-id 2026-10-30_15-48-03_cli
Mn9Patcher --cli --game-dir "..." --restore-saves   # latest
```

## Backups, locking and autosave

Config backups: `<game>/.mn9patcher/baseline/`.

With **Lock config** checked (default), the patcher marks the INIs and
`MN9Game/SaveData/SystemData.dat` read-only (Windows attribute / Unix `0444`).
That stops the game from saving graphics settings in-game and after exit, so the
patcher's Apply button is the only writer. Tested on Linux: the game still
launches with `SystemData.dat` locked and its mtime does not change.

Uncheck **Lock config** (or use Restore) if you want the in-game menu to save
changes instead.

## Post-process shaders

**Graphics → Effects → Post-process** is a single-choice picker. The old cel
shader was replaced by a retro pass (`libpostfx.so`, hooked at
`SDL_GL_SwapWindow`): nearest-neighbour pixelation to a virtual resolution,
colour quantisation, optional Bayer dithering, scanlines and vignette.

- **Off**
- **Retro — Soft** — 270p, 32 colour levels, no dither, light vignette
- **Retro — 8-bit** — 240p, 8 colour levels, Bayer dither, vignette
- **Retro — CRT** — 240p, 16 levels, dither, scanlines and stronger vignette
- **Noir** — grayscale, contrast, vignette and film grain
- **Dot Matrix** — green LED dots (180-row halftone grid, hue-tinted)
- **Cinematic** — teal/warm grade, vignette, grain and 2.39:1 letterbox bars
- **Posterize** — 6-level per-channel banding with a slight saturation lift
- **Metallic — smooth** (default) — polished steel grade, soft specular band,
  brushed sheen and a gentle moving highlight; no grain or hard banding

The effect runs from the very first frame — title, menus, loading screens and
gameplay all get the selected style. There is no stage-only gating.

Tuning (environment, read when the library loads):

```sh
MN9_RETRO_PRESET=soft|8bit|crt
MN9_RETRO_HEIGHT=240        # virtual vertical resolution
MN9_RETRO_LEVELS=8          # quantisation steps per channel
MN9_RETRO_DITHER=1          # 0/1 Bayer dither
MN9_RETRO_SCANLINES=0.55    # 0..1
MN9_RETRO_VIGNETTE=0.15     # 0..1
```

CLI: `--cel-shading --cel-strength bold` still works and now selects the
matching retro preset (`subtle`→Soft, `bold`→8-bit, `comic`→CRT).

### Retro textures

**Effects → Retro textures** (CLI `--retro-textures`) applies the look at the
texture level instead of the frame:

- `MinMagFilter=point` on world/character/weapon/vehicle/effects/skybox/UI
  texture groups (nearest-neighbour sampling, crisp pixels),
- `MaxLODSize=256` for world/character/weapon (128 for effects, 512 skybox),
- `MaxAnisotropy=1`, `bAllowHighQualityMaterials=False`, `DetailMode=1`.

This also cuts VRAM/RAM because the engine streams smaller mips. It combines
with the screen-space Retro presets; enable both for the strongest effect.

Unchecking it now restores the original filtering and mip limits
(`MaxLODSize=2048` world / 1024 character, anisotropic 16, `DetailMode=2`).
**FXAA** (`Effect → FXAA`, on by default) smooths model and texture edges in
the same post-process pass; it is independent of the game's MSAA/MLAA.

## Audio (PC build)

The **Audio** tab writes `.mn9charsel/audio.conf` and installs
`libaudiofx.so`, an OpenAL shim hooked at `alSourcePlay`. MP3 decoding uses
`libmpg123` because the Steam Linux Runtime's libsndfile 1.0.25 has no MP3
support; OGG/FLAC/WAV go through libsndfile.

- **Music** — defaults to **Random from folder**, scanning `~/Music`
  recursively (three levels). The dropdown lets the player choose Game, Mute,
  Random, Playlist (name order) or Shuffle (no-repeat bag). If the folder has
  no playable tracks, the game's own BGM is left on.
- **Browse** — the music/map/game-folder pickers are an in-app directory
  browser, avoiding the KDE/Xdg portal dialog that can freeze Dolphin.
- **Stage map** — optional `music.map` with one `map-name=file-or-folder`
  rule per line. The shim reads `MN9Game/Logs/Launch.log` for the current
  `LoadMap:` name and uses the matching track/folder; `default=` is the
  fallback. **Create template** writes the common map names.
- **SFX pitch** — Off, ±5%, ±10% or ±20% per sound.
- **Voice** — Game or Mute voice (keep subtitles). Mute uses the OpenAL buffer
  length (roughly 0.5–5 s) to leave short SFX and the music alone.
- **Duck music under dialog** — lowers music to 35% while a voice line is
  playing, then restores it.
- **Night mode** — soft-knee compression + makeup gain applied to every
  OpenAL PCM buffer (and replacement music), reducing peaks so quiet dialog
  is audible.
- **Levels** — independent music / SFX / voice gain (0–100%).

The game's own **Disable dialog** checkbox is the exact path: it mutes voice
and hides dialog text. Use it when you want both; use the Audio tab's voice
mute when you want to keep subtitles.

Limits: this is PC/OpenAL only — the ReXGlue port uses SDL audio and is not
affected. Stage mapping is name-based (read from `Launch.log`) and replaces
music at cue boundaries; SFX randomization is pitch-based rather than sample
swapping.

## Character select

The launcher's **Character select** dropdown writes:

```text
<game>/.mn9charsel/character
```

Values: `beck`, `gold`, `retro`, `call`, `ray`.

- **Set character** only writes the config.
- **Launch with character** writes the config and, on Linux, starts
  `mods/spawn_override/mn9-charselect.sh`. The wrapper sets
  `MN9_CHARACTER`/`MN9_CHARSEL_CONF` and `LD_PRELOAD`s
  `libmn9spawn.so`, which swaps the player pawn at spawn time.
- `gold`/`retro` are Beck costumes; `call` swaps both the pawn and
  controller; `ray` swaps the pawn but currently keeps Beck's controller
  because Ray's DLC controller crashes in its seek-free UI package.
- **Auto skip cutscenes** and **Disable dialog** are written to
  `.mn9charsel/options` and applied by the spawn override at runtime.
- The wrapper is found relative to the launcher, from
  `<game>/.mn9charsel/mn9-charselect.sh`, or via `MN9_CHARSEL_WRAPPER`.
- On Windows the selection is saved for a future native hook; the current
  spawn override is Linux-only.

## Turbo auto-fire (Linux PC build)

Holding the game's fire button keeps firing. The launcher writes
`.mn9charsel/turbo.conf` and the spawn wrapper preloads `libmn9turbo.so`.

- **Enable turbo auto-fire** — master switch.
- **Fire button** — X (default), A, B or Y. The left mouse button is handled
  automatically as well.
- **Rate** — 6, 8, 10 (default), 12, 15 or 20 taps per second.
- **Pad toggle** — optionally reassign **R3 (Direct Change 3)** to turn
  auto-fire on/off. While selected, R3 no longer performs Direct Change 3.
- **Toggle key** — optional keyboard key that turns auto-fire on/off while
  playing; blank means always on.

The shim toggles the fire button in the game's `kog::g_sdlControllers`
cache at the selected rate while the physical button is held, and gates
`SDL_GetMouseState` for mouse fire. It does not change game speed. Linux PC
build only (the Windows launcher has no injection shim).

## CLI

The GUI is primary, but a headless mode remains for scripting/testing:

```sh
Mn9Patcher --cli --game-dir "/path/to/Mighty No. 9" --fps 120 --res 1280x720
Mn9Patcher --cli --game-dir "..." --fullscreen          # native size, aspect follows
Mn9Patcher --cli --game-dir "..." --windowed --no-vsync
Mn9Patcher --cli --game-dir "..." --msaa --mlaa
Mn9Patcher --cli --game-dir "..." --laa
Mn9Patcher --cli --game-dir "..." --restore
Mn9Patcher --cli --game-dir "..." --mouse
Mn9Patcher --cli --game-dir "..." --launch
Mn9Patcher --cli --game-dir "..." --character gold
Mn9Patcher --cli --game-dir "..." --show-character
Mn9Patcher --cli --game-dir "..." --character retro --launch-character
Mn9Patcher --cli --game-dir "..." --character gold --launch
Mn9Patcher --cli --game-dir "..." --turbo --turbo-rate 10 --turbo-button x --turbo-pad r3 --turbo-key ""
Mn9Patcher --cli --game-dir "..." --no-turbo
```

## Safe Mode

`--launch` (or the button) starts Steam, waits for `MN9Game`, and records a
crash marker in `%LOCALAPPDATA%\Mn9Patcher\crash` (or
`~/.local/share/Mn9Patcher/crash`) for any exit except 0/130/143. The next
launch restores the backup and applies a conservative 62 fps config before
starting.

## In-game menu

The retail Options UI is Scaleform: `TitleMenu.upk` → `SwfMovie Title.gfx` with
`StartSystemOptions` / `SystemOptions` frames. Fullscreen, VSync, resolution,
shadows, Bloom, AA, AF, etc. already exist there and read the same config keys.

The patcher-only options (mouse fix, voice enable, intro movies, texture pool,
physics) are not rows in that movie. Adding them means editing `Title.gfx` and
its ActionScript, then re-embedding the SWF — a separate GFx UI mod, not a
config patch.

## Notes

- MSAA is left off by default: the game's MSAA path crashes or renders white on
  the builds tested. Use Bloom/FXAA or MLAA instead.
- The Windows executable has not been tested on a real Windows install yet.
- The Linux AppImage was tested against copies of the real Linux INIs.
- Steam "Verify integrity" restores the config; re-run the patcher.

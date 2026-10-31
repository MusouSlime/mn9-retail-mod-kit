# Mn9Patcher

Cross-platform GUI patcher for the retail **Mighty No. 9** PC build. It edits
the loose UE3 config files, backs them up, and can optionally patch the mouse
capture path. Builds as a Windows `.exe` and a Linux `.AppImage` from the same
Avalonia code.

No game data is included. The Windows release is x86-64 (PE32+); it runs on
64-bit Windows 10/11.

## Build

```sh
./build_win.sh          # dist/win-x64/Mn9Patcher.exe + dist/Mn9Patcher-win64.zip
./build_appimage.sh     # dist/Mn9Patcher-x86_64.AppImage
./build_win.sh linux    # dist/linux-x64/Mn9Patcher (folder build)
```

Requires the .NET 10 SDK. The AppImage needs `mksquashfs` and an AppImage
runtime (`appimage/runtime-x86_64`).

## Use

Run `Mn9Patcher.exe` (Windows) or `Mn9Patcher-x86_64.AppImage` (Linux).

- The game folder is detected from Steam; **Browse** lets you pick it manually.
- Tick the settings you want.
- Press **Apply settings** (or Ctrl+Enter).
- **Restore original** copies the pre-patch backup back and reverts the mouse
  patch.
- **Launch with Safe Mode** restores a known-good config if the previous exit
  was abnormal, then starts the game and watches it.

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
1920x1080, 1920x1200), frame cap, texture pool MB, physics substeps.

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

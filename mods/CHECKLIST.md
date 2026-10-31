# Retail mod checklist

Track of every change made to the retail install and its status.
Linux paths are under `~/.steam/steam/steamapps/common/Mighty No. 9`.

## Config profiles (`mods/lowspec`)

- [x] `low` — 960x540 @ 85%, shadows/post off
- [x] `potato` — 640x360 @ 75%, LOD 2
- [x] `extreme` — 480x270 @ 67%, decals off
- [x] `minimal` — 480x270 @ 60%, fog/HQ materials off (risky)
- [x] `lowest` — 640x360 @ 50%, 24 fps, all presets forced, windowed
- [x] `quality` — 1280x720 @ 100%, shadows 2048, Bloom/FXAA, 62 fps
- [x] `maximum` — 1920x1080 @ 100%, shadows 4096, full texture groups
- [x] `--msaa N`, `--screen-percentage N`, `--no-mlaa`, `--windowed`, `--res WxH`
- [x] Profiles applied to both `SDL2Linux-MN9*.ini` and `Default*.ini`
- [x] Config files locked read-only after `apply` (prevents the game saving a crashing setting)
- [x] `restore` unlocks and restores the four INIs from `.mn9lowspec/baseline/`

## Binary patches (Linux ELF only)

- [x] `mods/msaa_off/patch_msaa.py` — forces MSAA off in native `UMN9_SystemData`
  - `0xd43989` skip `1 << AntiAliasing` loop
  - `0xd439a1` force allow flag 0
  - `0xd43491` force INI loader `AntiAliasing=0`
  - backup: `Binaries/Linux/MN9Game.bin.x86_64.orig`
- [ ] `mods/msaa_resolve/patch_resolve.py` — NOP the D3D11 platform gate in
  `ResolveGBufferSurfaces`; **reverted**, forcing it crashes at `BootGame`
- [ ] True MSAA on Linux — blocked: `ResolveMSAADepthSurface` is a `ret` stub and
  the OpenGL build skips G-buffer resolves

## Presentation

- [x] Windowed 640x360 verified
- [x] Windowed 1280x720 verified
- [x] 1920x1080 fullscreen on Linux — works at the panel's native mode; non-native exclusive modes black-screen under XWayland (patcher now forces native size)
- [x] `bForceNoMovies=True` skips loading/startup movies in both
  `SDL2Linux-MN9Engine.ini` and `DefaultEngine.ini`
- [x] `LoadMapMovies=Loading.Loading` removed from both engine INIs — the
  loading-screen movie no longer plays
- [x] Publisher/logo boot timeline skipped in `BootGame.upk` — frame 1 arms a
  1 s `EndPlay` timer; title entry ~19 s -> ~14 s
- [ ] Skip the `OpDemo` intro **stage** (intentionally not done; only movies are skipped)

## Performance / frame rate

- [x] 24 fps cap (`lowest`)
- [x] 62 fps cap (`quality` / `maximum`)
- [x] 120 fps cap — `UseVsync=False`, `MaxSmoothedFrameRate=120` (display is 60 Hz; needs 120 Hz output to see it)
- [x] `MaxPhysicsSubsteps` exposed (1 low, 5 quality)
- [x] `MaxChannels` exposed (4 low, 32 quality)
- [x] Texture pool 96 / 256 / 512 MB

## Windows patcher

- [x] `mods/patcher` — .NET single-file console app, builds
  `dist/win-x64/Mn9Patcher.exe` (self-contained, no .NET install needed)
- [x] Profiles: Potato / Quality / Maximum; global voice off, movies off,
  MSAA off, MLAA/TAA off
- [x] Backups to `<game>\.mn9patcher\baseline`, restore, read-only lock
- [x] Safe Mode `--launch`: abnormal exit marker, next launch restores
  baseline + Quality (62 fps)
- [x] Creates `MN9*.ini` platform files from defaults on a fresh install
- [x] Linux AppImage — `dist/Mn9Patcher-x86_64.AppImage`, tested against copies
  of the real Linux INIs; auto-detects `SDL2Linux-MN9*.ini`
- [x] `--fps N` override (120 sets `UseVsync=False`)
- [ ] Test the exe on a real Windows install
- [ ] Windows mouse-capture behavior (the SDL patch is Linux-only)

## Stability

- [x] Voice disabled — `[VoIP] bHasVoiceEnabled=False` in the platform and
  default engine INIs
- [x] Safe Mode wrapper — `mods/safe_mode/safe_launch.sh`
  - before launch: if the previous run exited abnormally, `restore` then
    `apply --profile quality --msaa 1 --no-mlaa`
  - after launch: records exit codes other than 0/130/143 in
    `~/.local/state/mn9-safe-mode/crash`
  - tested with `false` (marker written) and `true` (config restored, marker cleared)
  - Steam launch options: `bash "/path/to/mods/safe_mode/safe_launch.sh" %command%`
- [ ] Install the Safe Mode launch option in Steam (user-side one-time setting)
- [x] Title-menu DLC crash — `MN9UITitleMenu.LoadDLC()`'s `AddDLCTitleCall()`
  call redirected to the empty `CheckDLCRay()` (`mods/ingame_options/patch_titlecall.py`)
  - fixes the SIGSEGV in `ULinkerLoad::CreateExport` ~20 s after boot
  - also skips the Pl03..Pl07 title-call voice loads

## In-game Other menu

- [x] `SwfMovie Option` repacked into `MN9Game.upk` (`mods/ingame_options/pack_option.py`)
- [x] `Lives Remaining` (`etc3`) restored; label string is `Lives Remaining`
- [x] **Cursor Hold** row added below it, default **ON**
  (`OptionNameMC.text.text.text`, no scroll needed)
- [x] ON = grab/hide; OFF = release/show, applied immediately through a native
  code cave (`mods/ingame_options/patch_free_mouse.py`)
- [x] AS uses `ASSetText` / `ASInputSetInputEnable`; Scaleform drops free-form
  callback names that are not UFunctions
- [x] Cave stack is ABI-aligned; unaligned `SDL_SetWindowGrab` crashed in
  `XSync`/`xcb_wait_for_reply`
- [x] Cursor Hold is session-only; resets to ON when the Options movie reloads
  (accepted; true `TOtherOptionData` field blocked by the cooked linker)
- [x] Verified the packed GFX is byte-identical to `Option_free_mouse.gfx`

## Mouse capture

- [x] `mods/no_mouse_grab/patch_mouse.py` — game PLT: `SDL_SetWindowGrab` no-op,
  `SDL_ShowCursor` always visible
- [x] `mods/no_mouse_grab/patch_sdl.py` — bundled `libSDL2-2.0.so.0`:
  grab/show/relative/capture/warp disabled, backup `.orig`
- [x] Cursor Hold ON/OFF verified in-game; the old unconditional SDL patches are reverted

## Test environment (deferred)

- [ ] Windows VM for patcher/game testing
  - QEMU 10.1 + KVM (AMD-V), OVMF, `swtpm`, GTK display all installed
  - no `virt-manager`; plain QEMU `-display gtk` works
  - ~85 GB free on `/var`; optional cleanup: Trash 4 GB, unused Podman images 4 GB, Gradle 2.3 GB, NuGet 1.1 GB
  - Windows 10 22H2 recommended (no TPM/Secure Boot); Win11 needs `swtpm` + OVMF
  - ISO not downloaded yet (~6 GB)
  - no GPU passthrough: good for patcher GUI, not representative for the game

## Challenge mode (research)

- [x] Maps: `CH_SINGLE_01..36`, `CH_Coop_01..24`, `CH_COMMON`
- [x] Game info: `MN9GameInfo_Challenge`, `MN9GameInfo_ChallengeCoop`
- [x] Content archetypes: `ChallengeSingleCD001..036`, `ChallengeCoopCD001..024`
  in `MN9Game.upk` (packages `ChallengeSingleCD` / `ChallengeCoopCD`)
- [x] Class fields: `UsableSpecialWeapon`, `bIsCall`, `bHasRanking`,
  `bIsStoryMap`, `bShowResult`, `ResultType`, `DamageCountPoints`,
  `Spw07SpinPoints`, `ClearBonusPoints`; no map/character field
- [x] Player spawn uses `MN9PlayerController.LoadCostumePawn()`, so skin/Ray
  mods apply; `?Character=Call` is parsed from the URL
- [x] Maps are separate `CH_*` packages; swapping them keeps
  `MN9MapInfo_Challenge` and is the practical route
- [ ] Try repurposing a `CH_SINGLE_*` slot for a custom/test map
- [ ] Unlock challenge rows without a cleared save (edit `MN9_SystemData` flags)

## Content experiments

- [x] `Mn9TexScan` reads UE3 v869 mip tables
- [x] `downscale_upk_textures.py` in-place mip shift — **unsafe**, crashes
  `Map10` with `malloc(): invalid size`
- [x] `shrink_upk_textures.py` removes mip entries — **unsafe**, shifts every
  inline bulk offset
- [x] `OpDemo_Persistent00.upk` downscale test — restored from backup
- [x] `Map09_RT_Persistent00.upk` downscale test — restored from backup
- [x] `Map10_Tutorial_Persistent00.upk` downscale test — restored from backup
- [x] Ray `DLCTitleCallPl03` packages moved aside — restored
- [ ] Format-aware DXT resampler (not started)

## Port (paused)

- [x] Options menu presets: `POTATO` / `BALANCED` / `QUALITY`
- [ ] D-pad / memcard / stage select work (separate session)

## Known issues

- [x] Title-menu crash — two causes fixed: forced MSAA off on Linux, and the
  missing-DLC `AddDLCTitleCall()` call skipped
- [x] Boot logo timeline skipped via a 1 s `EndPlay` timer in the Boot GFX
  (`BootGame.upk`, decompressed + `pack_swf.py`)
- [x] Cursor Hold state is session-only (accepted)
- [ ] Windows port of the config script (names, XAudio2, `attrib +R`)
- [x] 120 fps — engine runs at 120 (`UseVsync=False`,
  `MaxSmoothedFrameRate=120`); MangoHud showed 120 on the stage-select screen.
  The panel is 60 Hz only, so frames tear and there is no 120 Hz sync. A true
  120 Hz test needs a 120 Hz display/EDID mode.
- [x] Fullscreen + maximum quality at 1920x1080 — `mods/lowspec --profile
  maximum` (100% render, 4096 shadow maps, MLAA, 16x AF, 512 MB pool, VSync,
  62 cap). Verified in-game: 1920x1080 fullscreen, ~30% GPU, 2.1 GB VRAM.
  Fullscreen must use the panel's native mode; non-native sizes black-screen
  under XWayland (see `mods/lowspec/README.md`).
- [x] Patcher forces the native display mode whenever **Fullscreen** is
  selected (`NativeDisplaySize()` via XRandR / `GetSystemMetrics`); CLI has
  `--fullscreen`, `--windowed`, `--vsync`, `--no-vsync`, `--msaa`,
  `--mlaa`. Rebuilt Windows zip/AppImage.
- [ ] MSAA at maximum quality — test failed: `patch_resolve.py` (force the
  G-buffer resolve on OpenGL) + `MaxMultiSamples=4` segfaults ~7 s into the
  stage load. Binary reverted. Keep `MaxMultiSamples=1` + MLAA.
- [ ] SSAA 150% — test failed: `ScreenPercentage=150` in every preset makes
  the stage-select/main-menu 3D render black (only guide arrows survive).
  Kept at 100%. Could retry at 125 or with `UpscaleScreenPercentage` off.
- [x] Controller/D-pad single-device quirk — Steam Input exposes a virtual
  Valve pad next to the physical XInput device; use
  `mods/controller_fix/mn9_single_controller.sh` or disable Steam Input for
  the game (README has verification steps)
- [x] Steam voice error spam — disabled via `[VoIP] bHasVoiceEnabled=False`
- [x] 4:3 / 16:10 resolution presets in the patcher (1024x768, 1280x960,
  1440x1080, 1280x800, 1680x1050, 1920x1200)
- [x] 4:3 UI fill — `DefaultAspectRatio` + Scaleform `SM_ExactFit` fills a
  1280x960 window without cropping the menu (`mods/aspect_fix`, also applied
  by the patcher's resolution preset). 16:10 still letterboxes because the
  engine forces the wide bucket to 16:9 before GFx runs.
- [ ] 16:10 fill — engine-side wide-bucket clamp, not the GFx scale mode
- [x] In-stage HUD corners for 4:3 — `mods/hud_corners` moves the player
  gauge, score, timer and combo to the window edges
  (`apply_hud_corners.py`); world-anchored markers stay with the player
- [x] 4:3 device simulation uses the `potato` low-end profile at 1280x960
  (`mods/lowspec`, ScreenPercentage 75)
- [x] Save/`SystemData.dat` backup tooling — `mods/patcher` GUI + CLI
  (`--backup-saves`, `--list-saves`, `--restore-saves`); timestamped snapshots
  under `<game>/.mn9patcher/saves`, SHA-256 manifest, `before-restore` safety
  snapshot, newest 20 kept, read-only mode preserved

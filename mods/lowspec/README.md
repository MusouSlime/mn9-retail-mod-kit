# Low-resource mod for retail Mighty No. 9 (PC)

A reversible "potato mode" for the retail build. It does not touch packages or
save data: it patches the loose UE3 config files the game already reads.

```
MN9Game/Config/SDL2Linux-MN9SystemSettings.ini   render resolution, effects, LODs
MN9Game/Config/SDL2Linux-MN9Engine.ini           frame cap, texture pool, audio voices
MN9Game/Config/SDL2Linux-MN9Game.ini             physics substeps
MN9Game/Config/DefaultSystemSettings.ini         regeneration fallback
```

## Usage

```sh
# close the game first
./mn9_lowspec.py apply --profile lowest

# see what is active
./mn9_lowspec.py show

# preview without writing
./mn9_lowspec.py diff --profile lowest

# put the original files back
./mn9_lowspec.py restore
```

Profiles:

| profile | render | effects | pool | voices | cap |
|---|---|---|---|---|---|
| `low` | 960x540 @ 85% | shadows + post off | 128 MB | 16 | 30 |
| `potato` | 640x360 @ 75% | as above, LOD 2 | 96 MB | 8 | 30 |
| `extreme` | 480x270 @ 67% | decals off | 96 MB | 4 | 30 |
| `minimal` | 480x270 @ 60% | decals + fog off, HQ materials off | 96 MB | 4 | 30 |
| `lowest` | 640x360 @ 50% (320x180) | decals off, 24 fps, all presets forced | 96 MB | 4 | 24 |

`lowest` is the no-breakage floor: shadows/post off, decals off, draw distance
0.5, LOD 3/3, 24 fps cap, vsync off, 4 voices. Fog volumes, high-quality
materials, texture groups, the DLC and English localization are left intact.
It also rewrites `Low`/`Mid`/`High`/`Ultra` so the in-game quality menu cannot
raise the settings back up.

`minimal` is the aggressive variant: it also disables fog volumes and
`bAllowHighQualityMaterials` (documented to break water in Map04) and turns
vsync off.

The first `apply` copies the four patched INIs to
`MN9Game/Config/.mn9lowspec/baseline/`; `restore` copies them back. The backup is
never overwritten, so it always returns the pre-mod state.

## What is actually being changed

Rendering:

- `DynamicShadows=False`, `LightEnvironmentShadows=False`, shadow maps 64 px,
  no foreground/whole-scene dominant shadows.
- `DetailMode=0` (culls high-detail particle systems), `ParticleLODBias=3`,
  `SkeletalMeshLODBias=3`, `MaxDrawDistanceScale=0.5` for `lowest`.
- Bloom, motion blur, light shafts, lens flares, DOF, AO, radial blur,
  subsurface scattering, image reflections, MLAA and distortion off.
- `ScreenPercentage=50` with `UpscaleScreenPercentage=True`, so the scene
  renders at 320x180 and the HUD/menu stay at 640x360.
- `MaxAnisotropy=1`. Texture groups are deliberately **not** touched: clamping
  `TEXTUREGROUP_*` and shrinking the pool below the level's working set makes
  UE3 evict mips and render black or solid-colour surfaces.
- Decals off (`bStaticDecalsEnabled`, `bDynamicDecalsEnabled`, `StaticDecals`,
  `DynamicDecals`, `UnbatchedDecals`).

Memory:

- `[TextureStreaming] PoolSize=96` (from 160 MB), still above the level working
  set. `MinTextureResidentMipCount` stays at the shipped 7.

CPU / power:

- Frame cap 24 (`bSmoothFrameRate=True`, `MaxSmoothedFrameRate=24`).
- `[ALAudio.ALAudioDevice] MaxChannels=4` (from 32) — fewer mixed voices.
- `MaxPhysicsSubsteps=1` (from 5).
- `DetailMode=0` also removes a lot of per-frame particle work.

## Safe-by-default choices

- `FogVolumes=True`, `DynamicLights=True`, `DirectionalLightmaps=True`,
  `bAllowHighQualityMaterials=True`, `FloatingPointRenderTargets=True` and the
  shipped texture groups are left alone. These are the switches known to break
  water, lighting or texture streaming.
- DLC (Ray, RetroHero) and the English localization packages are not touched.

## Caveats

- **Fullscreen only works at the display's native mode.** On this Wayland /
  XWayland setup an exclusive fullscreen request for a non-native size (for
  example `Fullscreen=True` with `ResX=1280 ResY=960` on a 1920x1080 panel)
  leaves a black screen. `maximum` uses `1920x1080`, which maps cleanly. For
  other sizes use windowed/borderless or add a matching display mode.
- **Do not raise `ScreenPercentage` above 100 on this build.** At 150% the
  stage-select/main-menu 3D layer renders black (only the guide arrows and
  bottom prompt remain). Tested with 150 in every preset; reverted to 100.
- The game rewrites `SDL2Linux-MN9SystemSettings.ini` at exit from its saved
  options. `ResX`, `ResY` and `ScreenPercentage` stick, but quality sliders
  (`DetailMode`, `MaxAnisotropy`, LOD bias, shadows) can be replaced by the
  values stored in `SystemData.dat`. Set the in-game quality to **Low** once,
  then re-run `apply`.
- Lowering `ResX`/`ResY` with `Fullscreen=True` means the game renders at that
  size and upscales; on a 4:3 handheld use `ResX=640 ResY=480` instead.
- Steam "Verify integrity of game files" restores the INIs.
- 24 fps is a deliberate cap, not a limitation. Set
  `MaxSmoothedFrameRate=30` (or 60) if you want it higher.
- To drop audio entirely, add `-nosound` to the Steam launch options. The INI
  only reduces mixed voices.
- Removing DLC is the biggest memory win available without repacking assets
  (title RSS ~1.6 GB -> ~0.9 GB in testing), but it removes Ray/RetroHero.

## Measuring

```sh
./bench.sh 30 12 lowest
```

`bench.sh` launches through Steam, waits, then reports RSS, CPU cores used, GPU
busy and peak VRAM. GPU counters come from
`/sys/class/drm/card*/device/`, so they are system-wide; keep the desktop idle.

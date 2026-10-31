#!/usr/bin/env python3
"""Low-resource profile for the retail Mighty No. 9 PC build.

The game ships loose, editable UE3 INIs. This tool patches the three files that
control rendering cost, memory and audio voices, and keeps a pristine backup so
the install can be restored byte-for-byte.

Usage:
    ./mn9_lowspec.py apply  [--profile low|potato|extreme] [--game-dir DIR]
    ./mn9_lowspec.py restore [--game-dir DIR]
    ./mn9_lowspec.py show   [--game-dir DIR]
    ./mn9_lowspec.py diff   [--profile ...] [--game-dir DIR]
"""

from __future__ import annotations

import argparse
import json
import os
import re
import shutil
import sys
from pathlib import Path

SYS = "MN9Game/Config/SDL2Linux-MN9SystemSettings.ini"
DEFSYS = "MN9Game/Config/DefaultSystemSettings.ini"
DEFENG = "MN9Game/Config/DefaultEngine.ini"
ENG = "MN9Game/Config/SDL2Linux-MN9Engine.ini"
GAME = "MN9Game/Config/SDL2Linux-MN9Game.ini"
CONFIG_FILES = (SYS, DEFSYS, ENG, GAME, DEFENG)

BACKUP_DIR = ".mn9lowspec"
STATE = "applied.json"

# ---------------------------------------------------------------- profiles
# Keys are patched inside their exact INI section, so duplicates elsewhere in
# the file (there are several) are left alone.

COMMON_SYSTEM = {
    # shadows: off entirely
    "DynamicShadows": "False",
    "LightEnvironmentShadows": "False",
    "bEnableForegroundShadowsOnWorld": "False",
    "bEnableForegroundSelfShadowing": "False",
    "bAllowWholeSceneDominantShadows": "False",
    "bAllowBranchingPCFShadows": "False",
    "bAllowHardwareShadowFiltering": "False",
    "MinShadowResolution": "32",
    "MaxShadowResolution": "64",
    "MinPreShadowResolution": "8",
    "MaxWholeSceneDominantShadowResolution": "64",
    "ShadowFadeResolution": "16",
    "PreShadowFadeResolution": "8",
    "ShadowFadeExponent": "0.1",
    "UnbuiltNumWholeSceneDynamicShadowCascades": "1",
    "UnbuiltWholeSceneDynamicShadowRadius": "4096",
    "PreShadowResolutionFactor": "0.25",
    "ShadowTexelsPerPixel": "0.5",
    # post process
    "MotionBlur": "False",
    "MotionBlurPause": "False",
    "MotionBlurSkinning": "0",
    "DepthOfField": "False",
    "AmbientOcclusion": "False",
    "Bloom": "False",
    "bAllowLightShafts": "False",
    "LensFlares": "False",
    "AllowRadialBlur": "False",
    "AllowSubsurfaceScattering": "False",
    "AllowImageReflections": "False",
    "AllowImageReflectionShadowing": "False",
    "bAllowPostprocessMLAA": "False",
    "bAllowSeparateTranslucency": "False",
    "bAllowTemporalAA": "False",
    "MaxFilterBlurSampleCount": "1",
    # materials / lighting
    "Distortion": "False",
    "FilteredDistortion": "False",
    "bAllowDownsampledTranslucency": "False",
    "bAllowHighQualityMaterials": "True",  # False breaks water in Map04
    "SHSecondaryLighting": "True",
    "CompositeDynamicLights": "True",
    "DirectionalLightmaps": "True",
    "DynamicLights": "True",
    "FogVolumes": "True",
    # geometry / particles
    "DetailMode": "0",
    "MaxDrawDistanceScale": "0.65",
    "SkeletalMeshLODBias": "3",
    "ParticleLODBias": "2",
    "ShadowFilterQualityBias": "0",
    "bAllowFracturedDamage": "True",
    "FractureCullDistanceScale": "0.75",
    "NumFracturedPartsScale": "0.75",
    # sampling
    "MaxAnisotropy": "1",
    "MaxMultiSamples": "1",
    "bAllowD3D9MSAA": "False",
    "MobileEnableMSAA": "False",
    "TemporalAAMemoryReserve": "0",
    "HighPrecisionGBuffers": "False",
    # scaling
    "UpscaleScreenPercentage": "True",
    "Fullscreen": "True",
    "UseVsync": "True",
    "AllowPerFrameSleep": "True",
    "AllowPerFrameYield": "True",
}

PROFILES = {
    "low": {
        "system": {
            **COMMON_SYSTEM,
            "ResX": "960",
            "ResY": "540",
            "ScreenPercentage": "85",
            "MaxDrawDistanceScale": "0.85",
            "SkeletalMeshLODBias": "2",
            "ParticleLODBias": "1",
        },
        "engine": {
            "[Engine.Engine]": {
                "bSmoothFrameRate": "True",
                "MinSmoothedFrameRate": "22",
                "MaxSmoothedFrameRate": "30",
            },
            "[TextureStreaming]": {
                "PoolSize": "128",
                "MinTextureResidentMipCount": "7",
            },
            "[ALAudio.ALAudioDevice]": {"MaxChannels": "16"},
        },
        "game": {"[Engine.WorldInfo]": {"MaxPhysicsSubsteps": "2"}},
    },
    "potato": {
        "system": {
            **COMMON_SYSTEM,
            "ResX": "640",
            "ResY": "360",
            "ScreenPercentage": "75",
            "MaxDrawDistanceScale": "0.8",
            "SkeletalMeshLODBias": "2",
            "ParticleLODBias": "1",
            "DynamicDecals": "True",
            "StaticDecals": "True",
        },
        "engine": {
            "[Engine.Engine]": {
                "bSmoothFrameRate": "True",
                "MinSmoothedFrameRate": "20",
                "MaxSmoothedFrameRate": "30",
            },
            "[TextureStreaming]": {
                "PoolSize": "96",
                "MinTextureResidentMipCount": "7",
            },
            "[ALAudio.ALAudioDevice]": {"MaxChannels": "8"},
        },
        "game": {"[Engine.WorldInfo]": {"MaxPhysicsSubsteps": "1"}},
    },
    # Absolute maximum retail profile: 1080p fullscreen, 4K shadow maps,
    # full texture groups, MLAA (post-process AA), all effects on.
    # MSAA remains forced off by mods/msaa_off.
    "maximum": {
        "system": {
            "ResX": "1920",
            "ResY": "1080",
            "ScreenPercentage": "100",
            "Fullscreen": "True",
            "Borderless": "False",
            "UseVsync": "True",
            "DynamicLights": "True",
            "DynamicShadows": "True",
            "LightEnvironmentShadows": "True",
            "CompositeDynamicLights": "True",
            "SHSecondaryLighting": "True",
            "DirectionalLightmaps": "True",
            "MotionBlur": "True",
            "MotionBlurPause": "True",
            "MotionBlurSkinning": "1",
            "DepthOfField": "False",
            "AmbientOcclusion": "True",
            "Bloom": "True",
            "bAllowLightShafts": "True",
            "Distortion": "True",
            "FilteredDistortion": "True",
            "DropParticleDistortion": "False",
            "bAllowDownsampledTranslucency": "False",
            "bAllowSeparateTranslucency": "False",
            "SpeedTreeLeaves": "True",
            "SpeedTreeFronds": "True",
            "LensFlares": "True",
            "AllowRadialBlur": "True",
            "AllowSubsurfaceScattering": "True",
            "AllowImageReflections": "True",
            "AllowImageReflectionShadowing": "True",
            "FogVolumes": "True",
            "MaxFilterBlurSampleCount": "16",
            "SkeletalMeshLODBias": "0",
            "ParticleLODBias": "0",
            "DetailMode": "2",
            "MaxDrawDistanceScale": "1.0",
            "ShadowFilterQualityBias": "2",
            "MaxAnisotropy": "16",
            "MaxMultiSamples": "1",
            "bAllowD3D9MSAA": "False",
            "bAllowTemporalAA": "False",
            "bAllowPostprocessMLAA": "True",
            "MinShadowResolution": "1024",
            "MaxShadowResolution": "4096",
            "MaxWholeSceneDominantShadowResolution": "4096",
            "ShadowFadeResolution": "32",
            "PreShadowFadeResolution": "16",
            "bEnableForegroundShadowsOnWorld": "True",
            "bEnableForegroundSelfShadowing": "True",
            "bAllowWholeSceneDominantShadows": "True",
            "UnbuiltNumWholeSceneDynamicShadowCascades": "4",
            "bAllowFracturedDamage": "True",
            "bAllowHighQualityMaterials": "True",
            "HighPrecisionGBuffers": "False",
            "TEXTUREGROUP_World": "(MinLODSize=64,MaxLODSize=2048,LODBias=0,MinMagFilter=aniso,MipFilter=point,NumStreamedMips=2)",
            "TEXTUREGROUP_WorldNormalMap": "(MinLODSize=32,MaxLODSize=1024,LODBias=0,MinMagFilter=aniso,MipFilter=point,NumStreamedMips=0)",
            "TEXTUREGROUP_WorldSpecular": "(MinLODSize=32,MaxLODSize=1024,LODBias=0,MinMagFilter=aniso,MipFilter=point,NumStreamedMips=0)",
            "TEXTUREGROUP_Character": "(MinLODSize=256,MaxLODSize=1024,LODBias=0,MinMagFilter=aniso,MipFilter=point,NumStreamedMips=0)",
            "TEXTUREGROUP_CharacterNormalMap": "(MinLODSize=128,MaxLODSize=1024,LODBias=0,MinMagFilter=aniso,MipFilter=point,NumStreamedMips=0)",
            "TEXTUREGROUP_CharacterSpecular": "(MinLODSize=128,MaxLODSize=1024,LODBias=0,MinMagFilter=aniso,MipFilter=point,NumStreamedMips=0)",
            "TEXTUREGROUP_Effects": "(MinLODSize=128,MaxLODSize=512,LODBias=0,MinMagFilter=aniso,MipFilter=point,NumStreamedMips=0)",
            "TEXTUREGROUP_EffectsNotFiltered": "(MinLODSize=128,MaxLODSize=256,LODBias=0,MinMagFilter=aniso,MipFilter=point,NumStreamedMips=0)",
            "TEXTUREGROUP_Skybox": "(MinLODSize=1024,MaxLODSize=2048,LODBias=0,MinMagFilter=aniso,MipFilter=point,NumStreamedMips=0)",
            "TEXTUREGROUP_UI": "(MinLODSize=2048,MaxLODSize=2048,LODBias=0,MinMagFilter=aniso,MipFilter=point,NumStreamedMips=0)",
            "TEXTUREGROUP_LightMap": "(MinLODSize=32,MaxLODSize=2048,LODBias=0,MinMagFilter=aniso,MipFilter=point,NumStreamedMips=0)",
            "TEXTUREGROUP_RenderTarget": "(MinLODSize=128,MaxLODSize=2048,LODBias=0,MinMagFilter=aniso,MipFilter=point,NumStreamedMips=0)",
            "TEXTUREGROUP_ShadowMap": "(MinLODSize=128,MaxLODSize=2048,LODBias=0,MinMagFilter=aniso,MipFilter=point,NumStreamedMips=0)",
        },
        "engine": {
            "[Engine.Engine]": {
                "bSmoothFrameRate": "True",
                "MinSmoothedFrameRate": "30",
                "MaxSmoothedFrameRate": "62",
                "bStaticDecalsEnabled": "True",
                "bDynamicDecalsEnabled": "True",
                "bAllowMultiThreadedShaderCompile": "True",
            },
            "[TextureStreaming]": {
                "PoolSize": "512",
                "MinTextureResidentMipCount": "7",
            },
            "[ALAudio.ALAudioDevice]": {"MaxChannels": "32"},
            "[FullScreenMovie]": {"bForceNoMovies": "True"},
        },
        "game": {"[Engine.WorldInfo]": {"MaxPhysicsSubsteps": "5"}},
    },
    # High-quality profile. MSAA stays off because the game's MSAA path is
    # broken (white screen / crash); AA is done by the Bloom/FXAA path instead.
    "quality": {
        "system": {
            "ResX": "1280",
            "ResY": "720",
            "ScreenPercentage": "100",
            "Fullscreen": "False",
            "Borderless": "False",
            "UseVsync": "True",
            "DynamicLights": "True",
            "DynamicShadows": "True",
            "LightEnvironmentShadows": "True",
            "CompositeDynamicLights": "True",
            "SHSecondaryLighting": "True",
            "DirectionalLightmaps": "True",
            "MotionBlur": "True",
            "MotionBlurPause": "True",
            "MotionBlurSkinning": "1",
            "DepthOfField": "False",
            "AmbientOcclusion": "True",
            "Bloom": "True",
            "bAllowLightShafts": "True",
            "Distortion": "True",
            "FilteredDistortion": "True",
            "DropParticleDistortion": "False",
            "bAllowDownsampledTranslucency": "False",
            "SpeedTreeLeaves": "True",
            "SpeedTreeFronds": "True",
            "LensFlares": "True",
            "FogVolumes": "True",
            "MaxFilterBlurSampleCount": "16",
            "SkeletalMeshLODBias": "0",
            "ParticleLODBias": "0",
            "DetailMode": "2",
            "MaxDrawDistanceScale": "1.0",
            "ShadowFilterQualityBias": "2",
            "MaxAnisotropy": "16",
            "MaxMultiSamples": "1",
            "bAllowD3D9MSAA": "False",
            "bAllowTemporalAA": "False",
            "bAllowPostprocessMLAA": "False",
            "MinShadowResolution": "512",
            "MaxShadowResolution": "2048",
            "MaxWholeSceneDominantShadowResolution": "2048",
            "ShadowFadeResolution": "32",
            "PreShadowFadeResolution": "16",
            "bEnableForegroundShadowsOnWorld": "True",
            "bEnableForegroundSelfShadowing": "True",
            "bAllowWholeSceneDominantShadows": "True",
            "UnbuiltNumWholeSceneDynamicShadowCascades": "3",
            "bAllowFracturedDamage": "True",
            "bAllowHighQualityMaterials": "True",
            "HighPrecisionGBuffers": "False",
        },
        "engine": {
            "[Engine.Engine]": {
                "bSmoothFrameRate": "True",
                "MinSmoothedFrameRate": "30",
                "MaxSmoothedFrameRate": "62",
                "bStaticDecalsEnabled": "True",
                "bDynamicDecalsEnabled": "True",
                "bAllowMultiThreadedShaderCompile": "True",
            },
            "[TextureStreaming]": {
                "PoolSize": "256",
                "MinTextureResidentMipCount": "7",
            },
            "[ALAudio.ALAudioDevice]": {"MaxChannels": "32"},
            "[FullScreenMovie]": {"bForceNoMovies": "True"},
        },
        "game": {"[Engine.WorldInfo]": {"MaxPhysicsSubsteps": "5"}},
    },
    # Lowest settings that do not rely on known-breakage switches.
    # Keeps fog volumes, HQ materials, texture groups, DLC and English LOC.
    "lowest": {
        "all_presets": True,
        "system": {
            **COMMON_SYSTEM,
            "ResX": "640",
            "ResY": "360",
            "ScreenPercentage": "50",
            "Fullscreen": "False",
            "Borderless": "False",
            "MaxDrawDistanceScale": "0.5",
            "SkeletalMeshLODBias": "3",
            "ParticleLODBias": "3",
            "StaticDecals": "False",
            "DynamicDecals": "False",
            "UnbatchedDecals": "False",
            "UseVsync": "False",
        },
        "engine": {
            "[Engine.Engine]": {
                "bSmoothFrameRate": "True",
                "MinSmoothedFrameRate": "20",
                "MaxSmoothedFrameRate": "24",
                "bStaticDecalsEnabled": "False",
                "bDynamicDecalsEnabled": "False",
                "bAllowMultiThreadedShaderCompile": "True",
            },
            "[FullScreenMovie]": {
                "bForceNoMovies": "True",
            },
            "[TextureStreaming]": {
                "PoolSize": "96",
                "MinTextureResidentMipCount": "7",
            },
            "[ALAudio.ALAudioDevice]": {"MaxChannels": "4"},
        },
        "game": {"[Engine.WorldInfo]": {"MaxPhysicsSubsteps": "1"}},
    },
    "minimal": {
        "system": {
            **COMMON_SYSTEM,
            "ResX": "480",
            "ResY": "270",
            "ScreenPercentage": "60",
            "MaxDrawDistanceScale": "0.5",
            "SkeletalMeshLODBias": "3",
            "ParticleLODBias": "2",
            "StaticDecals": "False",
            "DynamicDecals": "False",
            "UnbatchedDecals": "False",
            "FogVolumes": "False",
            "bAllowHighQualityMaterials": "False",
            "UseVsync": "False",
        },
        "engine": {
            "[Engine.Engine]": {
                "bSmoothFrameRate": "True",
                "MinSmoothedFrameRate": "20",
                "MaxSmoothedFrameRate": "30",
                "bStaticDecalsEnabled": "False",
                "bDynamicDecalsEnabled": "False",
            },
            "[TextureStreaming]": {
                "PoolSize": "96",
                "MinTextureResidentMipCount": "7",
            },
            "[ALAudio.ALAudioDevice]": {"MaxChannels": "4"},
        },
        "game": {"[Engine.WorldInfo]": {"MaxPhysicsSubsteps": "1"}},
    },
    "extreme": {
        "system": {
            **COMMON_SYSTEM,
            "ResX": "480",
            "ResY": "270",
            "ScreenPercentage": "67",
            "MaxDrawDistanceScale": "0.5",
            "SkeletalMeshLODBias": "4",
            "ParticleLODBias": "3",
            "DynamicDecals": "False",
            "StaticDecals": "False",
            "UnbatchedDecals": "False",
        },
        "engine": {
            "[Engine.Engine]": {
                "bSmoothFrameRate": "True",
                "MinSmoothedFrameRate": "20",
                "MaxSmoothedFrameRate": "30",
            },
            "[TextureStreaming]": {
                "PoolSize": "96",
                "MinTextureResidentMipCount": "7",
            },
            "[ALAudio.ALAudioDevice]": {"MaxChannels": "4"},
        },
        "game": {"[Engine.WorldInfo]": {"MaxPhysicsSubsteps": "1"}},
    },
}


# ---------------------------------------------------------------- helpers
def find_game_dir(explicit: str | None) -> Path:
    if explicit:
        p = Path(explicit).expanduser()
    else:
        candidates = [
            Path.home() / ".steam/steam/steamapps/common/Mighty No. 9",
            Path.home() / ".local/share/Steam/steamapps/common/Mighty No. 9",
        ]
        p = next((c for c in candidates if (c / SYS).is_file()), None)
        if p is None:
            sys.exit("Could not find the game. Pass --game-dir.")
    if not (p / SYS).is_file():
        sys.exit(f"{p} does not look like a Mighty No. 9 install (missing {SYS}).")
    return p


def read_lines(path: Path) -> tuple[list[str], str]:
    text = path.read_text(encoding="utf-8", errors="surrogateescape")
    eol = "\r\n" if "\r\n" in text else "\n"
    return text.splitlines(keepends=True), eol


def patch_section(lines: list[str], section: str, kv: dict[str, str], eol: str) -> tuple[list[str], int]:
    header = f"[{section}]"
    out: list[str] = []
    i = 0
    found = False
    changed = 0
    while i < len(lines):
        line = lines[i]
        if line.strip() == header:
            found = True
            out.append(line)
            i += 1
            replaced: set[str] = set()
            while i < len(lines) and not lines[i].lstrip().startswith("["):
                cur = lines[i]
                m = re.match(r"^(\s*)([A-Za-z0-9_]+)\s*=", cur)
                if m and m.group(2) in kv:
                    key = m.group(2)
                    newline = f"{m.group(1)}{key}={kv[key]}{eol}"
                    if newline != cur:
                        changed += 1
                    out.append(newline)
                    replaced.add(key)
                else:
                    out.append(cur)
                i += 1
            missing = [k for k in kv if k not in replaced]
            if missing:
                if out and not out[-1].endswith(("\n", "\r")):
                    out[-1] += eol
                for key in missing:
                    out.append(f"{key}={kv[key]}{eol}")
                    changed += 1
            continue
        out.append(line)
        i += 1
    if not found:
        raise KeyError(f"section [{section}] not found")
    return out, changed


def check_game_not_running() -> None:
    hits = []
    for pid in os.listdir("/proc"):
        if not pid.isdigit():
            continue
        try:
            comm = Path(f"/proc/{pid}/comm").read_text().strip()
        except OSError:
            continue
        if comm.startswith("MN9Game.bin"):
            hits.append(pid)
    if hits:
        sys.exit(f"Mighty No. 9 is running (pid {', '.join(hits)}). Close it first.")


def backup(game: Path) -> Path:
    root = game / "MN9Game/Config" / BACKUP_DIR
    baseline = root / "baseline"
    if not baseline.is_dir():
        baseline.mkdir(parents=True)
        (root / "restore.json").write_text(
            json.dumps({"game_dir": str(game), "files": list(CONFIG_FILES)}, indent=2) + "\n"
        )
        print(f"backup: {baseline}")
    for rel in CONFIG_FILES:
        dst = baseline / Path(rel).name
        if not dst.is_file() and (game / rel).is_file():
            shutil.copy2(game / rel, dst)
    return root


def apply_profile(game: Path, profile: str, dry_run: bool = False, msaa: int | None = None, screen_percentage: int | None = None, no_mlaa: bool = False, windowed: bool = False, res: tuple[int, int] | None = None, fps: int | None = None) -> None:
    spec = PROFILES[profile]
    if not dry_run:
        check_game_not_running()
        root = backup(game)
        for rel in CONFIG_FILES:
            try:
                os.chmod(game / rel, 0o644)
            except OSError:
                pass
    else:
        root = None

    system_kv = dict(spec["system"])
    engine_spec = {k: dict(v) for k, v in spec["engine"].items()}
    # Global safety/QoL settings applied to every profile.
    engine_spec.setdefault("[VoIP]", {})["bHasVoiceEnabled"] = "False"
    engine_spec.setdefault("[FullScreenMovie]", {})["bForceNoMovies"] = "True"
    engine_spec.setdefault("[FullScreenMovie]", {})["bShouldStopMovieAtEndOfLoadMap"] = "TRUE"
    if fps is not None:
        eng = engine_spec.setdefault("[Engine.Engine]", {})
        eng["bSmoothFrameRate"] = "True"
        eng["MinSmoothedFrameRate"] = "30"
        eng["MaxSmoothedFrameRate"] = str(fps)
        if fps > 60:
            system_kv["UseVsync"] = "False"
    if msaa is not None:
        system_kv["MaxMultiSamples"] = str(msaa)
        system_kv["bAllowD3D9MSAA"] = "True" if msaa > 1 else "False"
    if screen_percentage is not None:
        system_kv["ScreenPercentage"] = str(screen_percentage)
    if no_mlaa:
        system_kv["bAllowPostprocessMLAA"] = "False"
    if windowed:
        system_kv["Fullscreen"] = "False"
        system_kv["Borderless"] = "False"
    if res is not None:
        system_kv["ResX"], system_kv["ResY"] = str(res[0]), str(res[1])

    # The active block and the "Low" preset are patched together so that
    # re-selecting Low quality in the options menu does not undo the mod.
    presets = ["SystemSettings", "SystemSettingsLow"]
    if spec.get("all_presets"):
        presets += ["SystemSettingsMid", "SystemSettingsHigh", "SystemSettingsUltra"]
    targets = [(SYS, sec, system_kv) for sec in presets]
    # Regenerate-safe: if the platform INI is deleted the game rebuilds it
    # from DefaultSystemSettings.ini, so patch that too.
    targets += [(DEFSYS, sec, system_kv) for sec in presets]
    for rel, section, kv in targets:
        path = game / rel
        try:
            os.chmod(path, 0o644)
        except OSError:
            pass
        lines, eol = read_lines(path)
        out, changed = patch_section(lines, section, kv, eol)
        print(f"{rel} [{section}]: {changed} value(s)")
        if not dry_run and changed:
            path.write_text("".join(out), encoding="utf-8", errors="surrogateescape")

    default_engine = {k: engine_spec[k] for k in ("[VoIP]", "[FullScreenMovie]") if k in engine_spec}
    for rel, sections in ((ENG, engine_spec), (DEFENG, default_engine), (GAME, spec.get("game", {}))):
        path = game / rel
        lines, eol = read_lines(path)
        total = 0
        for section, kv in sections.items():
            section = section.strip("[]")
            out, changed = patch_section(lines, section, kv, eol)
            lines = out
            total += changed
            print(f"{rel} [{section}]: {changed} value(s)")
        if not dry_run and total:
            path.write_text("".join(lines), encoding="utf-8", errors="surrogateescape")

    if dry_run:
        return

    (root / STATE).write_text(
        json.dumps(
            {
                "profile": profile,
                "game_dir": str(game),
                "res": f'{system_kv["ResX"]}x{system_kv["ResY"]}',
                "screen_percentage": system_kv["ScreenPercentage"],
                "fps_cap": engine_spec.get("[Engine.Engine]", {}).get("MaxSmoothedFrameRate", "?"),
                "texture_pool_mb": spec["engine"]["[TextureStreaming]"]["PoolSize"],
                "audio_channels": spec["engine"]["[ALAudio.ALAudioDevice]"]["MaxChannels"],
            },
            indent=2,
        )
        + "\n"
    )
    for rel in CONFIG_FILES:
        try:
            os.chmod(game / rel, 0o444)
        except OSError:
            pass
    print(f"\nprofile '{profile}' applied (config locked read-only).")
    print(f"Restore with: {Path(__file__).name} restore")


def restore(game: Path) -> None:
    check_game_not_running()
    root = game / "MN9Game/Config" / BACKUP_DIR
    baseline = root / "baseline"
    if not baseline.is_dir():
        sys.exit(f"No backup found at {baseline}")
    for rel in CONFIG_FILES:
        try:
            os.chmod(game / rel, 0o644)
        except OSError:
            pass
        shutil.copy2(baseline / Path(rel).name, game / rel)
        print(f"restored {rel}")
    for name in (STATE,):
        p = root / name
        if p.exists():
            p.unlink()
    print("restored original config")


def show(game: Path) -> None:
    path = game / SYS
    lines, _ = read_lines(path)
    want = {
        "ResX", "ResY", "ScreenPercentage", "MaxDrawDistanceScale", "DetailMode",
        "DynamicShadows", "LightEnvironmentShadows", "Bloom", "MotionBlur",
        "MaxShadowResolution", "SkeletalMeshLODBias", "ParticleLODBias", "MaxAnisotropy",
    }
    section = None
    print(f"{path}")
    for line in lines:
        s = line.strip()
        if s.startswith("[") and s.endswith("]"):
            section = s[1:-1]
            continue
        if section != "SystemSettings":
            continue
        m = re.match(r"([A-Za-z0-9_]+)\s*=\s*(.*)", s)
        if m and m.group(1) in want:
            print(f"  {m.group(1)}={m.group(2)}")
    eng = game / ENG
    lines, _ = read_lines(eng)
    section = None
    want_eng = {"MaxSmoothedFrameRate", "MinSmoothedFrameRate", "bSmoothFrameRate", "PoolSize", "MinTextureResidentMipCount", "MaxChannels"}
    print(f"{eng}")
    for line in lines:
        s = line.strip()
        if s.startswith("[") and s.endswith("]"):
            section = s[1:-1]
            continue
        if section not in ("Engine.Engine", "TextureStreaming", "ALAudio.ALAudioDevice"):
            continue
        m = re.match(r"([A-Za-z0-9_]+)\s*=\s*(.*)", s)
        if m and m.group(1) in want_eng and (section != "Engine.Engine" or m.group(1) in want_eng):
            if section == "Engine.Engine" and m.group(1) == "bSmoothFrameRate" and m.group(2) not in ("True", "TRUE", "true", "False", "FALSE", "false"):
                continue
            print(f"  [{section}] {m.group(1)}={m.group(2)}")
    state = game / "MN9Game/Config" / BACKUP_DIR / STATE
    if state.is_file():
        print(f"\nstate: {state.read_text().strip()}")


def main() -> None:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("command", choices=("apply", "restore", "show", "diff"))
    ap.add_argument("--profile", choices=sorted(PROFILES), default="potato")
    ap.add_argument("--msaa", type=int, choices=(1, 2, 4, 8), default=None,
                    help="override MaxMultiSamples (1=off)")
    ap.add_argument("--screen-percentage", type=int, default=None,
                    help="override ScreenPercentage (100=1:1, 150=1.5x SSAA)")
    ap.add_argument("--no-mlaa", action="store_true", help="disable post-process MLAA")
    ap.add_argument("--windowed", action="store_true", help="force windowed mode")
    ap.add_argument("--res", help="override resolution, e.g. 1600x900")
    ap.add_argument("--fps", type=int, default=None,
                    help="frame cap, e.g. 120 (also disables vsync above 60)")
    ap.add_argument("--game-dir")
    args = ap.parse_args()
    res = None
    if args.res:
        try:
            w, h = args.res.lower().split("x")
            res = (int(w), int(h))
        except ValueError:
            ap.error("--res must look like 1600x900")

    game = find_game_dir(args.game_dir)
    if args.command == "apply":
        apply_profile(game, args.profile, msaa=args.msaa, screen_percentage=args.screen_percentage, no_mlaa=args.no_mlaa, windowed=args.windowed, res=res, fps=args.fps)
    elif args.command == "diff":
        apply_profile(game, args.profile, dry_run=True, msaa=args.msaa, screen_percentage=args.screen_percentage, no_mlaa=args.no_mlaa, windowed=args.windowed, res=res, fps=args.fps)
    elif args.command == "restore":
        restore(game)
    else:
        show(game)


if __name__ == "__main__":
    main()

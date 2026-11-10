#!/usr/bin/env bash
# Launch Mighty No. 9 with the spawn override.
#
# Steam launch option:
#   bash "/path/to/mods/spawn_override/mn9-charselect.sh" %command%
#
# The character comes from $MN9_CHARACTER (override) or
# <game>/.mn9charsel/character (written by mn9char).
set -euo pipefail

HERE="$(cd "$(dirname "$0")" && pwd)"
GAME="${MN9_GAME_DIR:-$HOME/.steam/steam/steamapps/common/Mighty No. 9}"
LIB="$HERE/libmn9spawn.so"
FX="$HERE/libpostfx.so"
AUDIO="$HERE/libaudiofx.so"
CONF="${MN9_CHARSEL_CONF:-$GAME/.mn9charsel/character}"
BIN="$GAME/Binaries/Linux/MN9Game.bin.x86_64"

if [ ! -x "$BIN" ]; then
  echo "mn9-charselect: game binary not found: $BIN" >&2
  exit 1
fi
if [ ! -f "$LIB" ]; then
  echo "mn9-charselect: override library missing: $LIB" >&2
  exit 1
fi

char="${MN9_CHARACTER:-}"
if [ -z "$char" ] && [ -f "$CONF" ]; then
  char="$(head -n1 "$CONF" | tr -d '[:space:]')"
fi
case "$char" in
  gold|retro|beck|call|ray|megaxel) ;;
  "") char="beck" ;;
  *) echo "mn9-charselect: unknown character '$char' (beck/gold/retro/call/ray/megaxel)" >&2; exit 2 ;;
esac
skin=""
case "$char" in
  call) char="beck"; LIB="$HERE/libmn9spawn_megaxel.so" ;;
  ray) skin="ray"; char="beck" ;;
  megaxel) char="beck"; LIB="$HERE/libmn9spawn_megaxel.so" ;;
esac
export MN9_CHARACTER="$char"
export MN9_SKIN="$skin"
export MN9_GAME_DIR="$GAME"

OPTS="$GAME/.mn9charsel/options"
skip=0
dialog=0
cel=1
strength="metallic"
fxaa=1
if [ -f "$OPTS" ]; then
  skip="$(sed -n 's/^skip_cutscenes=//p' "$OPTS" | tail -n1)"
  dialog="$(sed -n 's/^disable_dialog=//p' "$OPTS" | tail -n1)"
  cel="$(sed -n 's/^cel_shading=//p' "$OPTS" | tail -n1)"
  strength="$(sed -n 's/^cel_strength=//p' "$OPTS" | tail -n1)"
  fxaa="$(sed -n 's/^fxaa=//p' "$OPTS" | tail -n1)"
fi
export MN9_SKIP_CUTSCENES="${skip:-0}"
export MN9_DISABLE_DIALOG="${dialog:-0}"
export MN9_CEL_SHADING="${cel:-0}"
export MN9_FXAA="${fxaa:-1}"
AC="$GAME/.mn9charsel/audio.conf"
audio=0; amusic=game; adir=""; amap=""; apitch=0; avoice=game; avp=1; avpr=0; avr=0; amg=1; asg=1; avg=1; aduck=1; anight=0; atv=game; amv=both
if [ -f "$AC" ]; then
  audio="$(sed -n 's/^enabled=//p' "$AC" | tail -n1)"
  amusic="$(sed -n 's/^music=//p' "$AC" | tail -n1)"
  adir="$(sed -n 's/^music_dir=//p' "$AC" | tail -n1)"
  amap="$(sed -n 's/^music_map=//p' "$AC" | tail -n1)"
  apitch="$(sed -n 's/^sfx_pitch=//p' "$AC" | tail -n1)"
  avoice="$(sed -n 's/^voice=//p' "$AC" | tail -n1)"
  avp="$(sed -n 's/^voice_pitch=//p' "$AC" | tail -n1)"
  avpr="$(sed -n 's/^voice_pitch_rand=//p' "$AC" | tail -n1)"
  avr="$(sed -n 's/^voice_random=//p' "$AC" | tail -n1)"
  amg="$(sed -n 's/^music_gain=//p' "$AC" | tail -n1)"
  asg="$(sed -n 's/^sfx_gain=//p' "$AC" | tail -n1)"
  avg="$(sed -n 's/^voice_gain=//p' "$AC" | tail -n1)"
  aduck="$(sed -n 's/^duck=//p' "$AC" | tail -n1)"
  anight="$(sed -n 's/^night=//p' "$AC" | tail -n1)"
  atv="$(sed -n 's/^title_voice=//p' "$AC" | tail -n1)"
  amv="$(sed -n 's/^music_variant=//p' "$AC" | tail -n1)"
fi
case "$strength" in
  noir)       export MN9_POSTFX_MODE=noir ;;
  dotmatrix)  export MN9_POSTFX_MODE=dotmatrix ;;
  cinematic)  export MN9_POSTFX_MODE=cinematic ;;
  posterize)  export MN9_POSTFX_MODE=posterize ;;
  metallic)   export MN9_POSTFX_MODE=metallic ;;
  comic)      export MN9_CEL_LEVELS=4; export MN9_CEL_OUTLINE=1.25; export MN9_CEL_SATURATION=1.55 ;;
  subtle)     export MN9_CEL_LEVELS=6; export MN9_CEL_OUTLINE=0.75; export MN9_CEL_SATURATION=1.22 ;;
  *)          export MN9_CEL_LEVELS=5; export MN9_CEL_OUTLINE=1.00; export MN9_CEL_SATURATION=1.35 ;;
esac

# Stage the libraries under a path without spaces; LD_PRELOAD splits on spaces.
RUNTIME="${XDG_RUNTIME_DIR:-/tmp}/mn9mod-$(id -u)"
mkdir -p "$RUNTIME"
ln -sf "$LIB" "$RUNTIME/libmn9spawn.so"
PRELOAD="$RUNTIME/libmn9spawn.so"
SKINLIB="$HERE/libmn9skin.so"
if [ -n "${skin:-}" ] && [ -f "$SKINLIB" ]; then
  ln -sf "$SKINLIB" "$RUNTIME/libmn9skin.so"
  PRELOAD="$PRELOAD:$RUNTIME/libmn9skin.so"
fi
if [ "$cel" = "1" ] && [ -f "$FX" ]; then
  ln -sf "$FX" "$RUNTIME/libpostfx.so"
  PRELOAD="$PRELOAD:$RUNTIME/libpostfx.so"
fi
if [ "${audio:-0}" = "1" ] && [ -f "$AUDIO" ]; then
  ln -sf "$AUDIO" "$RUNTIME/libaudiofx.so"
  PRELOAD="$PRELOAD:$RUNTIME/libaudiofx.so"
  export MN9_AUDIO_MUSIC="${amusic:-game}"
  export MN9_AUDIO_MUSIC_DIR="${adir:-}"
  export MN9_AUDIO_SFX_PITCH="${apitch:-0}"
  export MN9_AUDIO_VOICE="${avoice:-game}"
  export MN9_AUDIO_VOICE_PITCH="${avp:-1}"
  export MN9_AUDIO_VOICE_PITCH_RAND="${avpr:-0}"
  export MN9_AUDIO_VOICE_RANDOM="${avr:-0}"
  export MN9_AUDIO_MUSIC_GAIN="${amg:-1}"
  export MN9_AUDIO_SFX_GAIN="${asg:-1}"
  export MN9_AUDIO_VOICE_GAIN="${avg:-1}"
  export MN9_TITLE_VOICE="${atv:-game}"
  export MN9_AUDIO_MUSIC_VARIANT="${amv:-both}"
  export MN9_TITLE_VOICE_DIR="$GAME/.mn9charsel/titlevoices"
  export MN9_AUDIO_MUSIC_MAP="${amap:-}"
  export MN9_AUDIO_DUCK="${aduck:-1}"
  export MN9_AUDIO_NIGHT="${anight:-0}"
fi
PAL="$HERE/libmn9palette.so"
if [ -f "$PAL" ]; then
  ln -sf "$PAL" "$RUNTIME/libmn9palette.so"
  PRELOAD="$PRELOAD:$RUNTIME/libmn9palette.so"
fi
# Keep Steam's overlay first so Steam Input and the overlay keep working.
if [ -n "${LD_PRELOAD:-}" ]; then
  export LD_PRELOAD="$LD_PRELOAD:$PRELOAD"
else
  export LD_PRELOAD="$PRELOAD"
fi

cd "$GAME/Binaries/Linux"
echo "mn9-charselect: character=$char"
if [ "$#" -gt 0 ]; then
  exec "$@"
else
  exec ./MN9Game.bin.x86_64 -windowed
fi

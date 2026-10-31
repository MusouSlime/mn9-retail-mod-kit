#!/usr/bin/env bash
# Install the in-game Free Mouse row + Lives Remaining row into MN9Game.upk.
#
# Inputs (override with env vars):
#   MN9_UPK     retail MN9Game.upk (default: Steam Linux install)
#   OPTION_GFX  built GFX (default: Option_free_mouse.gfx next to this script)
#   WORK        scratch dir (default: /tmp/mn9_ingame)
#   DOTNET      dotnet binary (default: dotnet on PATH)
#
# Steps:
#   1. dump the export manifest of the package
#   2. replace the SwfMovie Option payload (shifts later exports)
#   3. redirect LoadDLC's AddDLCTitleCall() call to CheckDLCRay() so the
#      missing DLC title-call packages cannot crash the title menu
#
# The package is modified in place; keep a backup first.
set -euo pipefail

here=$(cd "$(dirname "$0")" && pwd)
MN9_UPK=${MN9_UPK:-$HOME/.steam/steam/steamapps/common/Mighty\ No.\ 9/MN9Game/CookedLinux/MN9Game.upk}
OPTION_GFX=${OPTION_GFX:-$here/Option_free_mouse.gfx}
WORK=${WORK:-/tmp/mn9_ingame}
DOTNET=${DOTNET:-dotnet}
EXPORTS_PROJ=${EXPORTS_PROJ:-$here/../../tools/ue3/Mn9Exports}

mkdir -p "$WORK"

"$DOTNET" "$EXPORTS_PROJ/bin/Release/net10.0/Mn9Exports.dll" \
  "$MN9_UPK" "$WORK/exports.jsonl"

python3 "$here/pack_option.py" \
  "$MN9_UPK" "$WORK/exports.jsonl" "$OPTION_GFX" "$WORK/MN9Game_option.upk"

cp "$WORK/MN9Game_option.upk" "$MN9_UPK"
chmod 755 "$MN9_UPK"

python3 "$here/patch_titlecall.py" apply "$MN9_UPK"
echo "installed $OPTION_GFX into $MN9_UPK"

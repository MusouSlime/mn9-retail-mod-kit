#!/usr/bin/env bash
# Build a Linux AppImage of Mn9Patcher.
set -euo pipefail
cd "$(dirname "$0")"

# Rebuild the Linux shims from source when the toolchain is present.
if [ -x ./build_mods.sh ] && [ -f postfx/postfx.cpp ] && [ -f audiofx/audiofx.c ] && [ -f turbo/turbo.c ]; then
  ./build_mods.sh >/dev/null
fi

DOTNET="${DOTNET:-dotnet}"
if ! command -v "$DOTNET" >/dev/null 2>&1 && [ -x "$HOME/dev/dotnet/dotnet" ]; then
  DOTNET="$HOME/dev/dotnet/dotnet"
fi

"$DOTNET" publish -c Release -r linux-x64 --self-contained true \
  -o dist/linux-x64

rm -rf AppDir
mkdir -p AppDir/usr/bin
cp -a dist/linux-x64/. AppDir/usr/bin/
# Bundle the self-contained ReXGlue runtime used by "Install the port from XBLA".
if [ -d port-runtime/linux-x64 ]; then
  mkdir -p AppDir/usr/bin/port-runtime
  cp -a port-runtime/linux-x64 AppDir/usr/bin/port-runtime/
fi
# Bundle the spawn override so "Launch with character" works from the AppImage.
SPAWN="${SPAWN_DIR:-./spawn_override}"
if [ ! -d "$SPAWN" ]; then SPAWN="../spawn_override"; fi
mkdir -p AppDir/usr/spawn_override
cp "$SPAWN/mn9-charselect.sh" "$SPAWN/libmn9spawn.so" AppDir/usr/spawn_override/
if [ -f "$SPAWN/libpostfx.so" ]; then cp "$SPAWN/libpostfx.so" AppDir/usr/spawn_override/; fi
if [ -f "$SPAWN/libaudiofx.so" ]; then cp "$SPAWN/libaudiofx.so" AppDir/usr/spawn_override/; fi
if [ -f "$SPAWN/libmn9palette.so" ]; then cp "$SPAWN/libmn9palette.so" AppDir/usr/spawn_override/; fi
if [ -f "$SPAWN/libmn9turbo.so" ]; then cp "$SPAWN/libmn9turbo.so" AppDir/usr/spawn_override/; fi
chmod +x AppDir/usr/spawn_override/mn9-charselect.sh
cp appimage/AppRun AppDir/AppRun
cp appimage/mn9patcher.desktop AppDir/
chmod +x AppDir/AppRun AppDir/usr/bin/Mn9Patcher

if command -v convert >/dev/null 2>&1; then
  convert -size 256x256 xc:'#0b1a2a' -fill '#5ce8ff' -gravity center \
    -pointsize 80 -annotate 0 'MN9' AppDir/mn9patcher.png
else
  # 1x1 transparent fallback
  printf '\x89PNG\r\n\x1a\n' > AppDir/mn9patcher.png
fi

rm -f dist/XelLauncher-x86_64.AppImage dist/Mn9Patcher-x86_64.AppImage dist/Mn9Patcher.squashfs
mksquashfs AppDir dist/Mn9Patcher.squashfs -root-owned -noappend -comp gzip >/dev/null
cat appimage/runtime-x86_64 dist/Mn9Patcher.squashfs > dist/XelLauncher-x86_64.AppImage
chmod +x dist/XelLauncher-x86_64.AppImage
ln -sf XelLauncher-x86_64.AppImage dist/Mn9Patcher-x86_64.AppImage
echo "built: $(pwd)/dist/XelLauncher-x86_64.AppImage"

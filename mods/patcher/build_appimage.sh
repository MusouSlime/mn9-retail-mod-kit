#!/usr/bin/env bash
# Build a Linux AppImage of Mn9Patcher.
set -euo pipefail
cd "$(dirname "$0")"

DOTNET="${DOTNET:-dotnet}"
if ! command -v "$DOTNET" >/dev/null 2>&1 && [ -x "$HOME/dev/dotnet/dotnet" ]; then
  DOTNET="$HOME/dev/dotnet/dotnet"
fi

"$DOTNET" publish -c Release -r linux-x64 --self-contained true \
  -o dist/linux-x64

rm -rf AppDir
mkdir -p AppDir/usr/bin
cp -a dist/linux-x64/. AppDir/usr/bin/
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

rm -f dist/Mn9Patcher-x86_64.AppImage dist/Mn9Patcher.squashfs
mksquashfs AppDir dist/Mn9Patcher.squashfs -root-owned -noappend -comp gzip >/dev/null
cat appimage/runtime-x86_64 dist/Mn9Patcher.squashfs > dist/Mn9Patcher-x86_64.AppImage
chmod +x dist/Mn9Patcher-x86_64.AppImage
echo "built: $(pwd)/dist/Mn9Patcher-x86_64.AppImage"

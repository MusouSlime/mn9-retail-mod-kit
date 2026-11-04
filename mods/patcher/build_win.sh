#!/usr/bin/env bash
# Build the Mighty No. 9 patcher as a self-contained single-file exe.
set -euo pipefail
cd "$(dirname "$0")"

DOTNET="${DOTNET:-dotnet}"
if ! command -v "$DOTNET" >/dev/null 2>&1 && [ -x "$HOME/dev/dotnet/dotnet" ]; then
  DOTNET="$HOME/dev/dotnet/dotnet"
fi
TARGET="${1:-win}"

case "$TARGET" in
  win)
    "$DOTNET" publish -c Release -r win-x64 --self-contained true \
      -p:PublishSingleFile=true -p:IncludeNativeLibrariesForSelfExtract=true \
      -p:EnableCompressionInSingleFile=true -o dist/win-x64
    if [ -d port-runtime/win-x64 ]; then
      mkdir -p dist/win-x64/port-runtime
      cp -a port-runtime/win-x64 dist/win-x64/port-runtime/
    fi
    if command -v zip >/dev/null 2>&1; then
      rm -f dist/Mn9Patcher-win64.zip
      (cd dist/win-x64 && zip -qr ../Mn9Patcher-win64.zip .)
    fi
    echo "built: $(pwd)/dist/win-x64/Mn9Patcher.exe"
    ;;
  linux)
    "$DOTNET" publish -c Release -r linux-x64 --self-contained true \
      -p:PublishSingleFile=true -p:EnableCompressionInSingleFile=true \
      -o dist/linux-x64
    echo "built: $(pwd)/dist/linux-x64/Mn9Patcher"
    ;;
  *)
    echo "usage: $0 [win|linux]" >&2
    exit 2
    ;;
esac

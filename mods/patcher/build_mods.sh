#!/usr/bin/env bash
# Build the Linux LD_PRELOAD mods used by the spawn wrapper.
set -euo pipefail
cd "$(dirname "$0")"

CC="${CC:-gcc}"
CXX="${CXX:-g++}"

mkdir -p spawn_override

# Post-process pass (OpenGL, hooked at SDL_GL_SwapWindow).
"$CXX" -O2 -shared -fPIC postfx/postfx.cpp -o spawn_override/libpostfx.so -ldl

# OpenAL music/SFX shim.
"$CC" -O2 -shared -fPIC audiofx/audiofx.c -o spawn_override/libaudiofx.so -ldl -lm
"$CC" -O2 -shared -fPIC skins/palette.c -o spawn_override/libmn9palette.so -ldl

# Fast-forward shim (scales AWorldInfo::TimeDilation).
"$CC" -O2 -shared -fPIC turbo/turbo.c -o spawn_override/libmn9turbo.so -ldl

ls -l spawn_override/libpostfx.so spawn_override/libaudiofx.so spawn_override/libmn9turbo.so

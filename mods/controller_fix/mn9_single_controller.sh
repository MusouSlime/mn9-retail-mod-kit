#!/usr/bin/env bash
# Launch Mighty No. 9 with only the physical controller visible to SDL2.
#
# Steam Input exposes a virtual "Valve Streaming Gamepad" next to the real
# XInput device.  SDL2 then reports two joysticks, and the game can bind the
# duplicate to a second local player, so the D-pad/buttons appear to work on
# only one device.
#
# Ignore the Valve virtual pad (VID 0x28de, PID 0x11ff).  Do not use this
# wrapper with a Steam Controller / Steam Deck if the virtual pad is the only
# device Steam exposes; use Steam Input only, or ignore the physical duplicate
# instead.
#
# Steam launch options:
#   bash "/absolute/path/to/mn9_single_controller.sh" %command%
#
# Keep any existing ignore list:
#   export SDL_GAMECONTROLLER_IGNORE_DEVICES="0x28de,0x11ff${SDL_GAMECONTROLLER_IGNORE_DEVICES:+,$SDL_GAMECONTROLLER_IGNORE_DEVICES}"

set -euo pipefail

export SDL_GAMECONTROLLER_IGNORE_DEVICES="${SDL_GAMECONTROLLER_IGNORE_DEVICES:-0x28de,0x11ff}"

exec "$@"

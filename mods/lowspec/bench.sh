#!/usr/bin/env bash
# Launch the retail build through Steam, wait for the title screen, and sample.
#
#   ./bench.sh [settle_seconds] [sample_seconds] [label]
#
# Steam must be running and logged in. The game is killed if already running.
set -uo pipefail

settle=${1:-30}
sample=${2:-12}
label=${3:-run}
here=$(cd "$(dirname "$0")" && pwd)

ps -eo pid=,comm= | awk '$2 ~ /^MN9Game/ {print $1}' | xargs -r kill 2>/dev/null
sleep 3

echo "launching..."
DISPLAY=${DISPLAY:-:0} steam -applaunch 314710 >/tmp/mn9-lowspec-launch.log 2>&1 &

for _ in $(seq 1 150); do
  if ps -eo comm= | grep -q '^MN9Game'; then break; fi
  sleep 1
done

if ! ps -eo comm= | grep -q '^MN9Game'; then
  echo "game did not start; see /tmp/mn9-lowspec-launch.log" >&2
  exit 1
fi

echo "settling ${settle}s..."
sleep "$settle"

"$here/measure.sh" "$sample" "$label"

echo
echo "config:"
"$here/mn9_lowspec.py" show

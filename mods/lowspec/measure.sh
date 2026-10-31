#!/usr/bin/env bash
# Sample CPU / RAM / GPU / VRAM of a running Mighty No. 9 process.
#
#   ./measure.sh <seconds> [label]
#
# Prints one summary block. GPU counters are read from sysfs when available;
# they are system-wide, so run with nothing else heavy on the GPU.
set -uo pipefail

secs=${1:-12}
label=${2:-run}

pid=$(ps -eo pid=,comm= | awk '$2 ~ /^MN9Game/ {print $1; exit}')
if [[ -z "${pid:-}" ]]; then
  echo "Mighty No. 9 is not running" >&2
  exit 1
fi

hz=$(getconf CLK_TCK)
gpu_busy=$(ls /sys/class/drm/card*/device/gpu_busy_percent 2>/dev/null | head -1)
vram_used=$(ls /sys/class/drm/card*/device/mem_info_vram_used 2>/dev/null | head -1)

read_cpu() { awk '{print $14+$15}' "/proc/$pid/stat"; }
read_rss() { awk '/^VmRSS:/ {print $2}' "/proc/$pid/status"; }

cpu0=$(read_cpu)
gpu_sum=0; gpu_n=0; vram_max=0; rss_sum=0; rss_max=0; n=0

for _ in $(seq 1 "$secs"); do
  sleep 1
  n=$((n + 1))
  rss=$(read_rss); rss=${rss:-0}
  rss_sum=$((rss_sum + rss)); (( rss > rss_max )) && rss_max=$rss
  if [[ -n "${gpu_busy:-}" ]]; then
    v=$(cat "$gpu_busy" 2>/dev/null || echo 0)
    gpu_sum=$((gpu_sum + v)); gpu_n=$((gpu_n + 1))
  fi
  if [[ -n "${vram_used:-}" ]]; then
    v=$(cat "$vram_used" 2>/dev/null || echo 0)
    (( v > vram_max )) && vram_max=$v
  fi
done

cpu1=$(read_cpu)
cores=$(awk -v d=$((cpu1 - cpu0)) -v hz="$hz" -v s="$secs" 'BEGIN {printf "%.2f", d / hz / s}')
rss_avg=$((rss_sum / (n > 0 ? n : 1)))

gpu_avg="n/a"
[[ ${gpu_n:-0} -gt 0 ]] && gpu_avg=$(awk -v s="$gpu_sum" -v n="$gpu_n" 'BEGIN {printf "%.0f%%", s / n}')

geom=$(DISPLAY=${DISPLAY:-:0} xdotool search --name 'Mighty' getwindowgeometry %@ 2>/dev/null | tr '\n' ' ' | sed 's/  */ /g')

printf '=== %s\n' "$label"
printf 'pid=%s  window=%s\n' "$pid" "${geom:-unknown}"
printf 'rss_avg=%.1f MiB  rss_peak=%.1f MiB\n' \
  "$(awk -v k="$rss_avg" 'BEGIN {print k/1024}')" \
  "$(awk -v k="$rss_max" 'BEGIN {print k/1024}')"
printf 'cpu_cores=%.2f  gpu=%s  vram_peak=%.0f MiB\n' \
  "$cores" "$gpu_avg" \
  "$(awk -v k="$vram_max" 'BEGIN {print k/1048576}')"

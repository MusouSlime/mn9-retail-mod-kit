#!/usr/bin/env bash
# Wrap the game command with crash recovery.
#
# Steam launch options:
#   bash "/path/to/mods/safe_mode/safe_launch.sh" %command%
#
# If the previous run exited abnormally (anything except 0, SIGINT, SIGTERM),
# the next launch restores the baseline config and reapplies the known-good
# Quality profile before starting.
set -u

here=$(cd "$(dirname "$0")" && pwd)
mod="$here/../lowspec/mn9_lowspec.py"
state="${XDG_STATE_HOME:-$HOME/.local/state}/mn9-safe-mode"
marker="$state/crash"
mkdir -p "$state"

if [[ -f "$marker" ]]; then
  echo "safe mode: previous crash $(cat "$marker") — restoring known-good config" >&2
  python3 "$mod" restore >/dev/null 2>&1 || true
  python3 "$mod" apply --profile quality --msaa 1 --no-mlaa >/dev/null 2>&1 || true
  rm -f "$marker"
fi

"$@"
rc=$?

case "$rc" in
  0|130|143) ;;  # clean exit, Ctrl-C, or normal SIGTERM from Steam
  *)
    printf '%s rc=%s\n' "$(date -Is)" "$rc" > "$marker"
    echo "safe mode: abnormal exit $rc recorded; next launch will use safe config" >&2
    ;;
esac

exit "$rc"

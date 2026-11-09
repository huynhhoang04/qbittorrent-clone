#!/usr/bin/env bash
# Start the Tracker server in the foreground so the caller can background it
# and keep ownership of the pid.
#   usage: start_tracker.sh [port]
set -euo pipefail

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
repo_root="${QBT_REPO_ROOT:-$(cd "$script_dir/.." && pwd)}"
tracker_bin="${QBT_TRACKER_BIN:-$build_dir/src/tracker/TrackerApp}"
if [ ! -f "$tracker_bin" ] && [ -f "${tracker_bin}.exe" ]; then
  tracker_bin="${tracker_bin}.exe"
fi

port="${1:-8080}"

if [ ! -f "$tracker_bin" ]; then
  echo "tracker binary not found: $tracker_bin" >&2
  exit 1
fi

exec "$tracker_bin" --port "$port"

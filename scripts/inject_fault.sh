#!/usr/bin/env bash
# Terminate a peer that a demo script started, to prove fault tolerance.
#   usage: inject_fault.sh <pid> [crash|stop]
#
# crash = SIGKILL, the process dies without running any cleanup, which is what
#         a real node loss looks like.
# stop  = SIGTERM, the peer shuts itself down cleanly.
#
# Only a pid handed to this script is touched; it never searches for and kills
# unrelated processes.
set -euo pipefail

pid="${1:?usage: inject_fault.sh <pid> [crash|stop]}"
mode="${2:-crash}"

case "$mode" in
  crash) signal=-9 ;;
  stop)  signal=-TERM ;;
  *) echo "unknown mode: $mode (expected crash|stop)" >&2; exit 2 ;;
esac

if ! kill -0 "$pid" 2>/dev/null; then
  echo "pid $pid is not running" >&2
  exit 1
fi

kill "$signal" "$pid"
echo "injected $mode (signal $signal) into pid $pid"

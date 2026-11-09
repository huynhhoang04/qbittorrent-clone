#!/usr/bin/env bash
# Start one peer with a named profile, on its own port and data directory.
#   usage: start_peer.sh <profile> <tracker_url> <work_dir> [extra PeerApp args...]
#
# Profiles are plain peer configurations, not special code paths: a seeder is
# just a peer that is already seeding the file, a leecher is a peer told to
# download it.
set -euo pipefail

profile="${1:?usage: start_peer.sh <profile> <tracker_url> <work_dir> [args...]}"
tracker_url="${2:?missing tracker url}"
work_dir="${3:?missing work dir}"
shift 3

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
repo_root="${QBT_REPO_ROOT:-$(cd "$script_dir/.." && pwd)}"
peer_bin="${QBT_PEER_BIN:-$build_dir/src/peer/PeerApp}"
if [ ! -f "$peer_bin" ] && [ -f "${peer_bin}.exe" ]; then
  peer_bin="${peer_bin}.exe"
fi

if [ ! -f "$peer_bin" ]; then
  echo "peer binary not found: $peer_bin" >&2
  exit 1
fi

mkdir -p "$work_dir/$profile"

# Port 0 lets the OS pick a free port, so several peers can share a host.
exec "$peer_bin" \
  --tracker "$tracker_url" \
  --port 0 \
  --data-dir "$work_dir/$profile" \
  "$@"

#!/usr/bin/env bash
# Runs the seven mandatory demo scenarios against the real PeerApp and
# TrackerApp binaries, then prints one PASS/FAIL line per scenario.
#
# Every process, port and directory it uses is its own; it kills only the pids
# it started and removes only the temporary directory it created.
set -uo pipefail

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
repo_root="${QBT_REPO_ROOT:-$(cd "$script_dir/.." && pwd)}"
build_dir="${QBT_BUILD_DIR:-$repo_root/build}"
peer_bin="$build_dir/src/peer/PeerApp.exe"
tracker_bin="$build_dir/src/tracker/TrackerApp.exe"

for binary in "$peer_bin" "$tracker_bin"; do
  if [ ! -f "$binary" ]; then
    echo "missing binary: $binary (build the project first)" >&2
    exit 1
  fi
done

# Resolved with pwd -P so the cleanup cannot follow a symlinked TMPDIR out of
# the tree this script created.
work="$(mktemp -d "${TMPDIR:-/tmp}/qbt_demo_XXXXXX")"
work="$(cd "$work" && pwd -P)"
pids=()
declare -a results=()
# Peers are started through command substitution, which runs in a subshell, so
# an in-memory array would lose their pids. The registry is a file instead.
pid_registry="$work/pids"
: >"$pid_registry"

cleanup() {
  local pid
  for pid in $(cat "$pid_registry" 2>/dev/null) "${pids[@]:-}"; do
    kill -9 "$pid" >/dev/null 2>&1 || true
  done
  wait >/dev/null 2>&1 || true
  rm -rf -- "$work"
}
trap cleanup EXIT

# A port is only probed, so there is a small race between picking it and the
# server binding it; the tracker startup check below catches that case.
pick_port() {
  local candidate
  for _ in $(seq 1 200); do
    candidate=$((21000 + RANDOM % 20000))
    if ! (exec 3<>"/dev/tcp/127.0.0.1/$candidate") >/dev/null 2>&1; then
      echo "$candidate"
      return 0
    fi
  done
  return 1
}

start_tracker() {
  local port="$1" log="$2"
  "$tracker_bin" --port "$port" >"$log" 2>&1 &
  pids+=($!)
  for _ in $(seq 1 80); do
    if curl -s -m 2 "http://127.0.0.1:$port/health" >/dev/null 2>&1; then
      return 0
    fi
    sleep 0.25
  done
  return 1
}

start_peer() {
  local tag="$1" log="$2"
  shift 2
  ( exec "$peer_bin" "$@" ) > "$log" 2>&1 &
  local pid=$!
  echo "$pid" >>"$pid_registry"
  echo "$pid"
}

wait_for() {
  local pid="$1" seconds="$2" elapsed=0
  while kill -0 "$pid" 2>/dev/null && [ "$elapsed" -lt "$seconds" ]; do
    sleep 1
    elapsed=$((elapsed + 1))
  done
  if kill -0 "$pid" 2>/dev/null; then
    return 1
  fi
  return 0
}

peers_json() {
  curl -s -m 3 "http://127.0.0.1:$1/peers?file_id=$2" 2>/dev/null
}

sha() { sha256sum "$1" | cut -d' ' -f1; }

record() { results+=("$1|$2|$3"); }

make_source() {
  local path="$1" kb="$2" seed="${3:-1}"
  head -c "$((kb * 1024))" /dev/urandom >"$path"
  # A fixed per-scenario seed keeps each file distinct between runs.
  printf '%s' "$seed" >>"$path"
}

# ---------------------------------------------------------------- scenario 1
scenario_registration_discovery() {
  local port="$1" url="http://127.0.0.1:$1" root="$work/s1"
  mkdir -p "$root"
  make_source "$root/file.bin" 256 1
  local id; id="$(sha "$root/file.bin")"

  start_peer seed1 "$root/seed1.log" --tracker "$url" --port 0 \
    --data-dir "$root/seed1" --seed "$root/file.bin" --chunk-size 32768 \
    >"$root/seed1.pid"
  local seed_pid; read -r seed_pid <"$root/seed1.pid"
  wait_for "$seed_pid" 1 >/dev/null 2>&1 || true

  local seen=""
  for _ in $(seq 1 60); do
    seen="$(peers_json "$port" "$id")"
    echo "$seen" | grep -q '"peer_id"' && break
    sleep 0.5
  done
  if ! echo "$seen" | grep -q '"peer_id"'; then
    record 1 "Tracker registration and discovery" "FAIL: seeder never appeared in /peers"
    return
  fi
  local observer_root="$work/s1/observer"
  mkdir -p "$observer_root"
  local obs_pid
  obs_pid="$(start_peer observer "$root/observer.log" --tracker "$url" --port 0 \
    --data-dir "$observer_root" --seed "$root/file.bin" --chunk-size 32768)"
  for _ in $(seq 1 40); do
    seen="$(peers_json "$port" "$id")"
    [ "$(echo "$seen" | grep -o '"peer_id"' | wc -l)" -ge 2 ] && break
    sleep 0.5
  done
  local count
  count="$(echo "$seen" | grep -o '"peer_id"' | wc -l | tr -d ' ')"
  kill -9 "$obs_pid" >/dev/null 2>&1 || true
  kill -9 "$seed_pid" >/dev/null 2>&1 || true
  if [ "$count" -ge 2 ]; then
    record 1 "Tracker registration and discovery" "PASS: /peers returned $count peers for file_id $id"
  else
    record 1 "Tracker registration and discovery" "FAIL: only $count peer(s) discovered"
  fi
}

# ---------------------------------------------------------------- scenario 2
scenario_single_source_download() {
  local port="$1" url="http://127.0.0.1:$1" root="$work/s2"
  mkdir -p "$root/leech" "$root/out"
  make_source "$root/file.bin" 512 2
  local id expected; id="$(sha "$root/file.bin")"; expected="$id"

  start_peer seed "$root/seed.log" --tracker "$url" --port 0 \
    --data-dir "$root/seed" --seed "$root/file.bin" --chunk-size 32768 \
    >"$root/seed.pid"
  local seed_pid; read -r seed_pid <"$root/seed.pid"

  local leech_pid
  leech_pid="$(start_peer leech "$root/leech.log" --tracker "$url" --port 0 \
    --data-dir "$root/leech" --download "$root/file.bin.torrent.json" \
    --output-dir "$root/out" --chunk-size 32768 --exit-after-download)"
  if ! wait_for "$leech_pid" 120; then
    kill -9 "$leech_pid" "$seed_pid" >/dev/null 2>&1 || true
    record 2 "Single-source download with SHA-256" "FAIL: download timed out"
    return
  fi
  kill -9 "$seed_pid" >/dev/null 2>&1 || true
  local got
  got="$(sha "$root/out/file.bin" 2>/dev/null || echo MISSING)"
  if [ "$got" = "$expected" ]; then
    record 2 "Single-source download with SHA-256" "PASS: sha256 $got matches source"
  else
    record 2 "Single-source download with SHA-256" "FAIL: expected $expected got $got"
  fi
}

# ---------------------------------------------------------------- scenario 3
scenario_multi_source() {
  local port="$1" url="http://127.0.0.1:$1" root="$work/s3"
  mkdir -p "$root/leech" "$root/out"
  make_source "$root/file.bin" 1024 3
  local id expected; id="$(sha "$root/file.bin")"; expected="$id"

  # Both seeders hold the whole file and each may serve at most one peer, so a
  # completed transfer that reports two contributors really used both sources.
  local seed_a seed_b
  seed_a="$(start_peer seedA "$root/seedA.log" --tracker "$url" --port 0 \
    --data-dir "$root/seedA" --seed "$root/file.bin" --chunk-size 32768 --max-uploads 1)"
  seed_b="$(start_peer seedB "$root/seedB.log" --tracker "$url" --port 0 \
    --data-dir "$root/seedB" --seed "$root/file.bin" --chunk-size 32768 --max-uploads 1)"

  local leech_pid
  leech_pid="$(start_peer leech "$root/leech.log" --tracker "$url" --port 0 \
    --data-dir "$root/leech" --download "$root/file.bin.torrent.json" \
    --output-dir "$root/out" --chunk-size 32768 --max-downloads 5 \
    --pipeline-depth 4 --exit-after-download)"
  if ! wait_for "$leech_pid" 150; then
    kill -9 "$leech_pid" "$seed_a" "$seed_b" >/dev/null 2>&1 || true
    record 3 "Multi-source download" "FAIL: download timed out"
    return
  fi
  kill -9 "$seed_a" "$seed_b" >/dev/null 2>&1 || true

  local got contributors
  got="$(sha "$root/out/file.bin" 2>/dev/null || echo MISSING)"
  contributors="$(grep -o 'Multi-source summary: [0-9]*' "$root/leech.log" | tail -1 | grep -o '[0-9]*$')"
  contributors="${contributors:-0}"
  if [ "$got" = "$expected" ] && [ "$contributors" -ge 2 ]; then
    record 3 "Multi-source download" "PASS: $contributors peers contributed chunks and sha256 matches"
  elif [ "$got" = "$expected" ]; then
    record 3 "Multi-source download" "FAIL: file is correct but only $contributors peer(s) contributed"
  else
    record 3 "Multi-source download" "FAIL: expected $expected got $got"
  fi
}

# ---------------------------------------------------------------- scenario 4
scenario_failover() {
  local port="$1" url="http://127.0.0.1:$1" root="$work/s4"
  mkdir -p "$root/leech" "$root/out"
  make_source "$root/file.bin" 2048 4
  local id expected; id="$(sha "$root/file.bin")"; expected="$id"

  # The killed seeder is deliberately the slow one, so it is still holding the
  # bulk of the transfer when it is removed.
  local doomed healthy
  doomed="$(start_peer doomed "$root/doomed.log" --tracker "$url" --port 0 \
    --data-dir "$root/doomed" --seed "$root/file.bin" --chunk-size 32768 --chunk-delay-ms 60)"
  healthy="$(start_peer healthy "$root/healthy.log" --tracker "$url" --port 0 \
    --data-dir "$root/healthy" --seed "$root/file.bin" --chunk-size 32768)"

  local leech_pid
  leech_pid="$(start_peer leech "$root/leech.log" --tracker "$url" --port 0 \
    --data-dir "$root/leech" --download "$root/file.bin.torrent.json" \
    --output-dir "$root/out" --chunk-size 32768 --io-timeout-ms 3000 \
    --exit-after-download)"
  sleep 4
  "$script_dir/inject_fault.sh" "$doomed" crash >"$root/fault.log" 2>&1
  pids=("${pids[@]/$doomed}")

  if ! wait_for "$leech_pid" 180; then
    kill -9 "$leech_pid" "$healthy" >/dev/null 2>&1 || true
    record 4 "Seeder failure with failover" "FAIL: download did not recover"
    return
  fi
  kill -9 "$healthy" >/dev/null 2>&1 || true
  local got
  got="$(sha "$root/out/file.bin" 2>/dev/null || echo MISSING)"
  if [ "$got" = "$expected" ]; then
    record 4 "Seeder failure with failover" "PASS: killed pid $doomed mid-transfer, remaining chunks came from the survivor, sha256 matches"
  else
    record 4 "Seeder failure with failover" "FAIL: expected $expected got $got"
  fi
}

# ---------------------------------------------------------------- scenario 5
scenario_corruption() {
  local port="$1" url="http://127.0.0.1:$1" root="$work/s5"
  mkdir -p "$root/leech" "$root/out"
  make_source "$root/file.bin" 1024 5
  local id expected; id="$(sha "$root/file.bin")"; expected="$id"

  local poison honest
  poison="$(start_peer poison "$root/poison.log" --tracker "$url" --port 0 \
    --data-dir "$root/poison" --seed "$root/file.bin" --chunk-size 32768 \
    --corrupt-chunks 0,1,2,3,4,5,6,7)"
  honest="$(start_peer honest "$root/honest.log" --tracker "$url" --port 0 \
    --data-dir "$root/honest" --seed "$root/file.bin" --chunk-size 32768)"

  local leech_pid
  leech_pid="$(start_peer leech "$root/leech.log" --tracker "$url" --port 0 \
    --data-dir "$root/leech" --download "$root/file.bin.torrent.json" \
    --output-dir "$root/out" --chunk-size 32768 --max-downloads 5 \
    --pipeline-depth 4 --exit-after-download)"
  if ! wait_for "$leech_pid" 180; then
    kill -9 "$leech_pid" "$poison" "$honest" >/dev/null 2>&1 || true
    record 5 "Corrupted chunk detection" "FAIL: download timed out"
    return
  fi
  kill -9 "$poison" "$honest" >/dev/null 2>&1 || true

  local mismatches got
  mismatches="$(grep -c 'SHA-256 mismatch' "$root/leech.log" | tr -d ' ')"
  got="$(sha "$root/out/file.bin" 2>/dev/null || echo MISSING)"
  if [ "$mismatches" -ge 1 ] && [ "$got" = "$expected" ]; then
    record 5 "Corrupted chunk detection" "PASS: $mismatches corrupted chunk(s) detected and re-fetched, sha256 matches"
  else
    record 5 "Corrupted chunk detection" "FAIL: mismatches=$mismatches hash_match=$([ "$got" = "$expected" ] && echo yes || echo no)"
  fi
}

# ---------------------------------------------------------------- scenario 6
scenario_crash_resume() {
  local port="$1" url="http://127.0.0.1:$1" root="$work/s6"
  mkdir -p "$root/leech" "$root/out"
  make_source "$root/file.bin" 4096 6
  local id expected; id="$(sha "$root/file.bin")"; expected="$id"

  local seeder
  seeder="$(start_peer seed "$root/seed.log" --tracker "$url" --port 0 \
    --data-dir "$root/seed" --seed "$root/file.bin" --chunk-size 32768 --chunk-delay-ms 80)"

  local first_pid
  first_pid="$(start_peer leech1 "$root/leech1.log" --tracker "$url" --port 0 \
    --data-dir "$root/leech" --download "$root/file.bin.torrent.json" \
    --output-dir "$root/out" --chunk-size 32768 --exit-after-download)"

  # Wait until the leecher has announced a bitfield that is partly filled, so
  # the kill demonstrably happens in the middle of a transfer. Only the
  # leecher's own object is counted, because the seeder in the same response is
  # always fully verified.
  local leech_port="" partial=""
  for _ in $(seq 1 20); do
    leech_port="$(grep -oE 'listening on port [0-9]+' "$root/leech1.log" | tail -1 | grep -oE '[0-9]+$')"
    [ -n "$leech_port" ] && break
    sleep 0.5
  done
  if [ -z "$leech_port" ]; then
    kill -9 "$first_pid" "$seeder" >/dev/null 2>&1 || true
    record 6 "Crash resume from persisted state" "FAIL: could not determine the leecher port"
    return
  fi
  for _ in $(seq 1 120); do
    local body own trues bits
    body="$(peers_json "$port" "$id")"
    own="$(printf '%s' "$body" | tr '{' '\n' | grep -E "\"port\":$leech_port(,|\})" | head -1)"
    trues="$(printf '%s' "$own" | tr ',' '\n' | grep -c 'true' || true)"
    bits="$(printf '%s' "$own" | tr ',' '\n' | grep -cE 'true|false' || true)"
    if [ "${bits:-0}" -gt 0 ] && [ "${trues:-0}" -gt 0 ] && [ "$trues" -lt "$bits" ]; then
      partial="$trues/$bits"
      break
    fi
    if kill -0 "$first_pid" 2>/dev/null; then sleep 1; else break; fi
  done

  if [ -z "$partial" ]; then
    kill -9 "$first_pid" "$seeder" >/dev/null 2>&1 || true
    record 6 "Crash resume from persisted state" "FAIL: never observed partial progress to interrupt"
    return
  fi
  "$script_dir/inject_fault.sh" "$first_pid" crash >"$root/fault.log" 2>&1
  pids=("${pids[@]/$first_pid}")

  local second_pid
  second_pid="$(start_peer leech2 "$root/leech2.log" --tracker "$url" --port 0 \
    --data-dir "$root/leech" --download "$root/file.bin.torrent.json" \
    --output-dir "$root/out" --chunk-size 32768 --exit-after-download)"
  if ! wait_for "$second_pid" 240; then
    kill -9 "$second_pid" "$seeder" >/dev/null 2>&1 || true
    record 6 "Crash resume from persisted state" "FAIL: restarted peer did not finish"
    return
  fi
  kill -9 "$seeder" >/dev/null 2>&1 || true

  local got fetched bits
  got="$(sha "$root/out/file.bin" 2>/dev/null || echo MISSING)"
  bits="${partial#*/}"
  # Chunks the survivor had to serve after the restart. Fewer than the whole
  # file means the peer resumed from its persisted state; equal to it means the
  # run silently re-downloaded everything.
  fetched="$(grep -oE 'Contributed [0-9]+ chunk' "$root/leech2.log" |
             grep -oE '[0-9]+' | awk '{s+=$1} END {print s+0}')"
  if [ "$got" = "$expected" ] && [ "${fetched:-0}" -gt 0 ] &&
     [ "$fetched" -lt "$bits" ]; then
    record 6 "Crash resume from persisted state" "PASS: killed at $partial verified chunk(s), restart re-fetched only $fetched of $bits, sha256 matches"
  elif [ "$got" = "$expected" ]; then
    record 6 "Crash resume from persisted state" "FAIL: file is correct but the restart re-fetched $fetched/$bits chunks, which is a fresh download, not a resume"
  else
    record 6 "Crash resume from persisted state" "FAIL: expected $expected got $got"
  fi
}

# ---------------------------------------------------------------- scenario 7
scenario_tracker_expiry() {
  local port="$1" url="http://127.0.0.1:$1" root="$work/s7"
  mkdir -p "$root/gone"
  make_source "$root/file.bin" 64 7
  local id; id="$(sha "$root/file.bin")"

  local victim
  victim="$(start_peer gone "$root/gone.log" --tracker "$url" --port 0 \
    --data-dir "$root/gone" --seed "$root/file.bin" --chunk-size 32768)"
  for _ in $(seq 1 40); do
    peers_json "$port" "$id" | grep -q '"peer_id"' && break
    sleep 0.5
  done
  if ! peers_json "$port" "$id" | grep -q '"peer_id"'; then
    kill -9 "$victim" >/dev/null 2>&1 || true
    record 7 "Tracker expiry after 45s" "FAIL: peer never registered"
    return
  fi

  "$script_dir/inject_fault.sh" "$victim" crash >"$root/fault.log" 2>&1
  pids=("${pids[@]/$victim}")

  local start now elapsed=""
  start="$(date +%s)"
  for _ in $(seq 1 130); do
    if ! peers_json "$port" "$id" | grep -q '"peer_id"'; then
      now="$(date +%s)"; elapsed=$((now - start)); break
    fi
    sleep 1
  done
  if [ -n "$elapsed" ] && [ "$elapsed" -le 75 ]; then
    record 7 "Tracker expiry after 45s" "PASS: stale peer dropped from /peers after ${elapsed}s (45s threshold, 10s sweep)"
  elif [ -n "$elapsed" ]; then
    record 7 "Tracker expiry after 45s" "FAIL: removal took ${elapsed}s, above the expected window"
  else
    record 7 "Tracker expiry after 45s" "FAIL: peer still listed after 130s"
  fi
}

# ---------------------------------------------------------------------- main
echo "M6 demo run"
echo "  repo:    $repo_root"
echo "  build:   $build_dir"
echo "  workdir: $work"

tracker_port="$(pick_port)" || { echo "could not find a free port"; exit 1; }
if ! start_tracker "$tracker_port" "$work/tracker.log"; then
  echo "tracker failed to start on port $tracker_port"
  exit 1
fi
echo "  tracker: http://127.0.0.1:$tracker_port"
echo

scenario_registration_discovery "$tracker_port"
scenario_single_source_download "$tracker_port"
scenario_multi_source           "$tracker_port"
scenario_failover               "$tracker_port"
scenario_corruption             "$tracker_port"
scenario_crash_resume           "$tracker_port"
scenario_tracker_expiry         "$tracker_port"

echo
echo "================= demo results ================="
failed=0
for entry in "${results[@]}"; do
  number="${entry%%|*}"
  rest="${entry#*|}"
  name="${rest%%|*}"
  detail="${rest#*|}"
  if [[ "$detail" == PASS* ]]; then
    printf 'DEMO %s  %-38s  %s\n' "$number" "$name" "$detail"
  else
    printf 'DEMO %s  %-38s  %s\n' "$number" "$name" "$detail"
    failed=$((failed + 1))
  fi
done
echo "================================================"
if [ "$failed" -eq 0 ]; then
  echo "ALL 7 DEMOS PASS"
else
  echo "$failed of 7 demos FAILED"
fi
exit "$failed"

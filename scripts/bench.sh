#!/usr/bin/env bash
# Performance / scalability experiment for the course report.
#
# Measures a verified end-to-end download (process start -> process exit, with a
# SHA-256 check of the result) under three independent dimensions:
#
#   A. Sources      1, 2 and 3 seeders, fixed file size   -> source scaling
#   B. File size    10, 50, 100 MB from one seeder        -> fixed overhead
#   C. Concurrency  1, 4, 8 simultaneous leechers         -> swarm stress
#
# Localhost disk is far faster than the network code, so a bare run of a 100 MB
# file finishes in half a second and every configuration looks identical. The
# seeders therefore serve with a fixed per-chunk delay, which makes the upload
# path the bottleneck the way a real WAN link would be.
#
# Prints a markdown table on stdout and optionally CSV (BENCH_CSV=path).
# Every process, port and directory it uses is its own; it kills only the pids
# it started and removes only the temporary directory it created.
set -uo pipefail

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
repo_root="${QBT_REPO_ROOT:-$(cd "$script_dir/.." && pwd)}"
build_dir="${QBT_BUILD_DIR:-$repo_root/build-release}"

resolve_bin() {
  if [ -f "$1" ]; then
    echo "$1"
  elif [ -f "$1.exe" ]; then
    echo "$1.exe"
  else
    echo "$1"
  fi
}

peer_bin="$(resolve_bin "$build_dir/src/peer/PeerApp")"
tracker_bin="$(resolve_bin "$build_dir/src/tracker/TrackerApp")"

for binary in "$peer_bin" "$tracker_bin"; do
  if [ ! -f "$binary" ]; then
    echo "missing binary: $binary" >&2
    echo "build the Release tree first, or set QBT_BUILD_DIR" >&2
    exit 1
  fi
done

work="$(mktemp -d "${TMPDIR:-/tmp}/qbt_bench_XXXXXX")"
work="$(cd "$work" && pwd -P)"
pid_registry="$work/pids"
: >"$pid_registry"

CHUNK_SIZE="${BENCH_CHUNK_SIZE:-262144}"
CHUNK_DELAY_MS="${BENCH_CHUNK_DELAY_MS:-50}"
SLOW_FILE_MB="${BENCH_FILE_MB:-100}"
RUN_TIMEOUT_S="${BENCH_TIMEOUT_S:-300}"

cleanup() {
  local pid
  for pid in $(cat "$pid_registry" 2>/dev/null); do
    kill -9 "$pid" >/dev/null 2>&1 || true
  done
  wait >/dev/null 2>&1 || true
  rm -rf -- "$work"
}
trap cleanup EXIT

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

now_us() { date +%s%6N; }

# Starts a peer in the background and stores its pid in the variable named by
# the first argument. A command substitution would run the process in a subshell
# and detach it, so the pid is handed back through a nameref instead.
start_peer() {
  local -n pid_out="$1"
  local log="$2"
  shift 2
  ( exec "$peer_bin" "$@" ) >"$log" 2>&1 &
  pid_out=$!
  # Without disown the shell prints a "Killed" job notice for every peer this
  # script deliberately terminates, which buries the result table.
  disown "$pid_out" 2>/dev/null || true
  echo "$pid_out" >>"$pid_registry"
}

# A reparented process is not ours to reap, and in a container pid 1 may never
# reap it either, so "still running" means "exists and is not a zombie".
pid_alive() {
  local pid="$1" state
  if [ -r "/proc/$pid/status" ]; then
    state="$(awk '/^State:/ {print $2}' "/proc/$pid/status" 2>/dev/null)"
    [ -n "$state" ] && [ "$state" != "Z" ]
    return
  fi
  kill -0 "$pid" 2>/dev/null
}

wait_pid() {
  local pid="$1" waited=0
  while pid_alive "$pid"; do
    if [ "$waited" -ge $((RUN_TIMEOUT_S * 20)) ]; then
      kill -9 "$pid" >/dev/null 2>&1 || true
      return 1
    fi
    sleep 0.05
    waited=$((waited + 1))
  done
  return 0
}

# Peak resident set size in kilobytes, sampled every 50 ms until the process is
# gone. VmHWM is a high-water mark, so the last readable value is the peak.
peak_rss_kb() {
  local pid="$1" peak=0 value
  while pid_alive "$pid"; do
    value="$(awk '/^VmHWM:/ {print $2}' "/proc/$pid/status" 2>/dev/null)"
    [ -n "${value:-}" ] && peak="$value"
    sleep 0.05
  done
  echo "$peak"
}

peers_json() { curl -s -m 3 "http://127.0.0.1:$1/peers?file_id=$2" 2>/dev/null; }

sha() { sha256sum "$1" | cut -d' ' -f1; }

make_file() {
  local path="$1" mb="$2"
  head -c "$((mb * 1024 * 1024))" /dev/urandom >"$path"
}

wait_for_file() {
  local file="$1"
  for _ in $(seq 1 600); do
    [ -f "$file" ] && return 0
    sleep 0.1
  done
  return 1
}

wait_for_peers() {
  local port="$1" file_id="$2" wanted="$3"
  local body count
  for _ in $(seq 1 200); do
    body="$(peers_json "$port" "$file_id")"
    count="$(printf '%s' "$body" | grep -o '"peer_id"' | wc -l | tr -d ' ')"
    [ "${count:-0}" -ge "$wanted" ] && return 0
    sleep 0.1
  done
  return 1
}

declare -a rows=()

record() {
  # record <dimension> <label> <bytes> <elapsed_ms> <contributors> <rss_kb> <verified>
  rows+=("$1|$2|$3|$4|$5|$6|$7")
}

# Runs one leecher to completion, timing it and sampling its peak RSS.
# Prints "<elapsed_ms>|<verified>" where verified is "yes" or "NO:<reason>".
run_leecher() {
  # run_leecher <leecher_data_dir> <torrent> <out_dir> <log> <expected_hash>
  local data_dir="$1" torrent="$2" out_dir="$3" log="$4" expected="$5"
  local pid start end rss

  start="$(now_us)"
  start_peer pid "$log" --tracker "$url" --port 0 --data-dir "$data_dir" \
    --download "$torrent" --output-dir "$out_dir" --chunk-size "$CHUNK_SIZE" \
    --max-downloads 8 --pipeline-depth 4 --exit-after-download
  rss="$(peak_rss_kb "$pid")"
  local finished=0
  wait_pid "$pid" || finished=1
  end="$(now_us)"

  local got verified="yes"
  got="$(sha "$out_dir/$(basename "${torrent%.torrent.json}")" 2>/dev/null || echo MISSING)"
  if [ "$finished" -ne 0 ]; then
    verified="NO:timed out"
  elif [ "$got" != "$expected" ]; then
    verified="NO:sha256"
  elif ! grep -q 'Multi-source summary' "$log" 2>/dev/null; then
    verified="NO:no completion log"
  fi
  echo "$(( (end - start) / 1000 ))|$rss|$verified"
}

# ------------------------------------------------------------------ dimension A
# Source scaling: every seeder is limited to one upload slot, so a multi-source
# result can only be produced if each seeder really contributed chunks.
bench_sources() {
  local mb="$SLOW_FILE_MB" sources
  for sources in 1 2 3; do
    local root="$work/a$sources"
    mkdir -p "$root/leech" "$root/out"
    make_file "$root/file.bin" "$mb"
    local expected
    expected="$(sha "$root/file.bin")"

    local pids=() pid i
    for i in $(seq 1 "$sources"); do
      start_peer pid "$root/seed$i.log" --tracker "$url" --port 0 \
        --data-dir "$root/seed$i" --seed "$root/file.bin" --chunk-size "$CHUNK_SIZE" \
        --max-uploads 1 --chunk-delay-ms "$CHUNK_DELAY_MS"
      pids+=("$pid")
    done
    if ! wait_for_file "$root/file.bin.torrent.json"; then
      record sources "$sources-source" 0 -1 0 0 "NO:torrent metadata never appeared"
      for pid in "${pids[@]}"; do kill -9 "$pid" >/dev/null 2>&1 || true; done
      continue
    fi
    wait_for_peers "$tracker_port" "$expected" "$sources" || true

    local result
    result="$(run_leecher "$root/leech" "$root/file.bin.torrent.json" "$root/out" "$root/leech.log" "$expected")"
    local elapsed="${result%%|*}" rest="${result#*|}"
    local rss="${rest%%|*}" verified="${rest#*|}"

    local contributors
    contributors="$(grep -o 'Multi-source summary: [0-9]*' "$root/leech.log" | tail -1 | grep -o '[0-9]*$')"
    contributors="${contributors:-0}"

    for pid in "${pids[@]}"; do kill -9 "$pid" >/dev/null 2>&1 || true; done
    record sources "$sources-source" "$((mb * 1024 * 1024))" "$elapsed" "$contributors" "$rss" "$verified"
  done
}

# ------------------------------------------------------------------ dimension B
# File-size scaling with a single source: shows the fixed per-transfer overhead
# and whether throughput holds as the transfer grows.
bench_sizes() {
  local mb
  for mb in 10 50 100; do
    local root="$work/b$mb"
    mkdir -p "$root/leech" "$root/out"
    make_file "$root/file.bin" "$mb"
    local expected
    expected="$(sha "$root/file.bin")"

    local seeder
    start_peer seeder "$root/seed.log" --tracker "$url" --port 0 \
      --data-dir "$root/seed" --seed "$root/file.bin" --chunk-size "$CHUNK_SIZE" \
      --max-uploads 1 --chunk-delay-ms "$CHUNK_DELAY_MS"
    if ! wait_for_file "$root/file.bin.torrent.json"; then
      record filesize "${mb}MB" 0 -1 0 0 "NO:torrent metadata never appeared"
      kill -9 "$seeder" >/dev/null 2>&1 || true
      continue
    fi
    wait_for_peers "$tracker_port" "$expected" 1 || true

    local result
    result="$(run_leecher "$root/leech" "$root/file.bin.torrent.json" "$root/out" "$root/leech.log" "$expected")"
    local elapsed="${result%%|*}" rest="${result#*|}"
    local rss="${rest%%|*}" verified="${rest#*|}"

    kill -9 "$seeder" >/dev/null 2>&1 || true
    record filesize "${mb}MB" "$((mb * 1024 * 1024))" "$elapsed" 1 "$rss" "$verified"
  done
}

# ------------------------------------------------------------------ dimension C
# Swarm stress: several leechers share one seeder. Run twice, once with a
# seeder that has one upload slot (every extra leecher must queue) and once with
# eight (the delays overlap). Every download must still verify.
bench_concurrency() {
  local mb=20 slots leechers
  for slots in 1 8; do
    for leechers in 1 4 8; do
      local root="$work/c${slots}x${leechers}"
      mkdir -p "$root"
      make_file "$root/file.bin" "$mb"
      local expected
      expected="$(sha "$root/file.bin")"

      local seeder
      start_peer seeder "$root/seed.log" --tracker "$url" --port 0 \
        --data-dir "$root/seed" --seed "$root/file.bin" --chunk-size "$CHUNK_SIZE" \
        --max-uploads "$slots" --chunk-delay-ms "$CHUNK_DELAY_MS"
      if ! wait_for_file "$root/file.bin.torrent.json"; then
        record concurrency "${leechers}-leecher/${slots}-slot" 0 -1 0 0 \
          "NO:torrent metadata never appeared"
        kill -9 "$seeder" >/dev/null 2>&1 || true
        continue
      fi
      wait_for_peers "$tracker_port" "$expected" 1 || true

      local i pid pids=() starts=() start end
      start="$(now_us)"
      for i in $(seq 1 "$leechers"); do
        mkdir -p "$root/leech$i" "$root/out$i"
        starts+=("$(now_us)")
        start_peer pid "$root/leech$i.log" --tracker "$url" --port 0 \
          --data-dir "$root/leech$i" --download "$root/file.bin.torrent.json" \
          --output-dir "$root/out$i" --chunk-size "$CHUNK_SIZE" --max-downloads 8 \
          --pipeline-depth 4 --exit-after-download
        pids+=("$pid")
      done
      local timed_out=0 idx=0 slowest=0
      for pid in "${pids[@]}"; do
        wait_pid "$pid" || timed_out=$((timed_out + 1))
        local done_at
        done_at="$(now_us)"
        local each=$(( (done_at - starts[idx]) / 1000 ))
        [ "$each" -gt "$slowest" ] && slowest=$each
        idx=$((idx + 1))
      done
      end="$(now_us)"

      local verified="yes" i2 got
      for i2 in $(seq 1 "$leechers"); do
        got="$(sha "$root/out$i2/file.bin" 2>/dev/null || echo MISSING)"
        [ "$got" = "$expected" ] || { verified="NO:leecher$i2 sha256"; break; }
      done
      [ "$timed_out" -eq 0 ] || verified="NO:$timed_out timed out"
      kill -9 "$seeder" >/dev/null 2>&1 || true
      # Time column is the slowest leecher, which is the wall-clock the swarm
      # took to deliver the file to everyone. Bytes is the whole swarm's
      # payload, so the throughput column is aggregate, not per-leecher.
      record concurrency "${leechers}-leecher/${slots}-slot" \
        "$((mb * 1024 * 1024 * leechers))" "$slowest" "-" 0 "$verified"
    done
  done
}

# ---------------------------------------------------------------------- main
tracker_port="$(pick_port)" || { echo "could not find a free port" >&2; exit 1; }
"$tracker_bin" --port "$tracker_port" >"$work/tracker.log" 2>&1 &
tracker_pid=$!
disown "$tracker_pid" 2>/dev/null || true
echo "$tracker_pid" >>"$pid_registry"
url="http://127.0.0.1:$tracker_port"
for _ in $(seq 1 80); do
  curl -s -m 2 "$url/health" >/dev/null 2>&1 && break
  sleep 0.25
done

echo "bench: build=$build_dir chunk_size=$((CHUNK_SIZE / 1024))KB chunk_delay=${CHUNK_DELAY_MS}ms"
echo

bench_sources
bench_sizes
bench_concurrency

echo
echo "| Dimension | Configuration | Data (MB) | Time (s) | Throughput (MB/s) | Contributors | Peak RSS (MB) | SHA-256 verified |"
echo "|---|---|---:|---:|---:|---:|---:|---|"
for entry in "${rows[@]}"; do
  IFS='|' read -r dimension label bytes ms contributors rss verified <<<"$entry"
  if [ "$ms" -lt 0 ] 2>/dev/null; then
    echo "| $dimension | $label | - | FAILED | - | - | - | $verified |"
    continue
  fi
  read -r mbs data_mb <<<"$(awk -v ms="$ms" -v b="$bytes" 'BEGIN {
    if (ms <= 0) { printf "0 0.0" } else {
      printf "%.1f %.1f", (b / 1048576.0) / (ms / 1000.0), b / 1048576.0
    } }')"
  rss_mb="-"
  [ "${rss:-0}" -gt 0 ] 2>/dev/null && rss_mb="$(awk -v k="$rss" 'BEGIN {printf "%.1f", k/1024}')"
  echo "| $dimension | $label | $data_mb | $(awk -v ms="$ms" 'BEGIN {printf "%.2f", ms/1000}') | $mbs | $contributors | $rss_mb | $verified |"
done

if [ -n "${BENCH_CSV:-}" ]; then
  echo "dimension,configuration,bytes,elapsed_ms,contributors,peak_rss_kb,verified" >"$BENCH_CSV"
  for entry in "${rows[@]}"; do
    echo "${entry//|/,}" >>"$BENCH_CSV"
  done
  echo
  echo "csv written to $BENCH_CSV"
fi

#!/usr/bin/env bash
# Helpers shared by the seven local, real-process demo scenarios.
# Sourced by demo_full.sh; run a numbered script to demonstrate one feature.

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
  printf '[MỞ PEER] %s (log: %s)\n' "$tag" "$log" >&2
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

wait_for_file() {
  local file="$1"
  for _ in $(seq 1 100); do
    [ -f "$file" ] && return 0
    sleep 0.1
  done
  return 1
}

peers_json() {
  curl -s -m 3 "http://127.0.0.1:$1/peers?file_id=$2" 2>/dev/null
}

# Return the leecher's announced progress (e.g. 10/129), never a seeder's.
wait_for_partial() {
  local tracker_port="$1" file_id="$2" log="$3" pid="$4"
  local peer_port="" body own trues bits
  for _ in $(seq 1 120); do
    if [ -z "$peer_port" ]; then
      peer_port="$(grep -oE 'listening on port [0-9]+' "$log" | tail -1 | grep -oE '[0-9]+$')"
    fi
    if [ -n "$peer_port" ]; then
      body="$(peers_json "$tracker_port" "$file_id")"
      own="$(printf '%s' "$body" | tr '{' '\n' | grep -E "\"port\":$peer_port(,|\})" | head -1)"
      trues="$(printf '%s' "$own" | tr ',' '\n' | grep -c 'true' || true)"
      bits="$(printf '%s' "$own" | tr ',' '\n' | grep -cE 'true|false' || true)"
      if [ "${bits:-0}" -gt 0 ] && [ "${trues:-0}" -gt 0 ] && [ "$trues" -lt "$bits" ]; then
        printf '%s/%s\n' "$trues" "$bits"
        return 0
      fi
    fi
    kill -0 "$pid" 2>/dev/null || return 1
    sleep 0.25
  done
  return 1
}

sha() { sha256sum "$1" | cut -d' ' -f1; }

record() {
  results+=("$1|$2|$3")
  printf '\nDEMO %s — %s\n%s\n' "$1" "$2" "$3"
  if [[ "$3" == FAIL* ]]; then keep_logs=1; fi
}

make_source() {
  local path="$1" kb="$2" seed="${3:-1}"
  head -c "$((kb * 1024))" /dev/urandom >"$path"
  # Append a scenario marker; the contents remain random on every run.
  printf '%s' "$seed" >>"$path"
}


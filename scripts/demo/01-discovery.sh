#!/usr/bin/env bash
# DEMO 1: ĐĂNG KÝ VÀ TÌM PEER
# Thao tác: Mở 2 peer seed cùng một file, hỏi tracker qua /peers.
# PASS khi: Tracker trả về ít nhất 2 peer cho đúng file_id.
# Chạy: bash scripts/demo/01-discovery.sh
# Các tiến trình chạy trên một máy qua localhost; tự dọn sau khi kết thúc.

if [[ "${BASH_SOURCE[0]}" == "$0" ]]; then
  exec bash "$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)/demo_full.sh" 1 "$@"
fi

demo_title='ĐĂNG KÝ VÀ TÌM PEER'
demo_action='Mở 2 peer seed cùng một file, hỏi tracker qua /peers.'
demo_expect='Tracker trả về ít nhất 2 peer cho đúng file_id.'
demo_function=scenario_registration_discovery

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

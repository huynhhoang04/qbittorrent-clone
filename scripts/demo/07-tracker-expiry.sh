#!/usr/bin/env bash
# DEMO 7: TRACKER LOẠI PEER MẤT LIÊN LẠC
# Thao tác: Cho peer đăng ký rồi tắt cưỡng bức; theo dõi /peers khoảng 1 phút.
# PASS khi: Peer biến mất trong 75 giây; TTL 45 giây, quét mỗi 10 giây.
# Chạy: bash scripts/demo/07-tracker-expiry.sh
# Các tiến trình chạy trên một máy qua localhost; tự dọn sau khi kết thúc.

if [[ "${BASH_SOURCE[0]}" == "$0" ]]; then
  exec bash "$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)/demo_full.sh" 7 "$@"
fi

demo_title='TRACKER LOẠI PEER MẤT LIÊN LẠC'
demo_action='Cho peer đăng ký rồi tắt cưỡng bức; theo dõi /peers khoảng 1 phút.'
demo_expect='Peer biến mất trong 75 giây; TTL 45 giây, quét mỗi 10 giây.'
demo_function=scenario_tracker_expiry

scenario_tracker_expiry() {
  local port="$1" url="http://127.0.0.1:$1" root="$work/s7"
  mkdir -p "$root/gone"
  make_source "$root/file.bin" 64 7
  local id; id="$(sha "$root/file.bin")"

  start_peer gone "$root/gone.log" --tracker "$url" --port 0 \
    --data-dir "$root/gone" --seed "$root/file.bin" --chunk-size 32768 \
    >"$root/gone.pid"
  local victim; read -r victim <"$root/gone.pid"
  victim="$(echo "$victim" | tr -dc '0-9')"
  for _ in $(seq 1 40); do
    peers_json "$port" "$id" | grep -q '"peer_id"' && break
    sleep 0.5
  done
  if ! peers_json "$port" "$id" | grep -q '"peer_id"'; then
    kill -9 "$victim" >/dev/null 2>&1 || true
    record 7 "Tracker expiry after 45s" "FAIL: peer never registered"
    return
  fi

  printf '[GÂY LỖI] Tắt peer; chờ tracker loại peer hết hạn (khoảng 1 phút).\n'
  if ! bash "$script_dir/inject_fault.sh" "$victim" crash >"$root/fault.log" 2>&1; then
    record 7 "Tracker expiry after 45s" "FAIL: could not crash the registered peer"
    return
  fi
  kill -9 "$victim" >/dev/null 2>&1 || true

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

#!/usr/bin/env bash
# DEMO 2: TẢI FILE TỪ MỘT NGUỒN
# Thao tác: Mở 1 seeder và 1 leecher; leecher tải bằng metadata.
# PASS khi: SHA-256 file tải về trùng file gốc.
# Chạy: bash scripts/demo/02-single-source.sh
# Các tiến trình chạy trên một máy qua localhost; tự dọn sau khi kết thúc.

if [[ "${BASH_SOURCE[0]}" == "$0" ]]; then
  exec bash "$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)/demo_full.sh" 2 "$@"
fi

demo_title='TẢI FILE TỪ MỘT NGUỒN'
demo_action='Mở 1 seeder và 1 leecher; leecher tải bằng metadata.'
demo_expect='SHA-256 file tải về trùng file gốc.'
demo_function=scenario_single_source_download

scenario_single_source_download() {
  local port="$1" url="http://127.0.0.1:$1" root="$work/s2"
  mkdir -p "$root/leech" "$root/out"
  make_source "$root/file.bin" 512 2
  local id expected; id="$(sha "$root/file.bin")"; expected="$id"

  start_peer seed "$root/seed.log" --tracker "$url" --port 0 \
    --data-dir "$root/seed" --seed "$root/file.bin" --chunk-size 32768 \
    >"$root/seed.pid"
  local seed_pid; read -r seed_pid <"$root/seed.pid"
  wait_for_file "$root/file.bin.torrent.json"

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

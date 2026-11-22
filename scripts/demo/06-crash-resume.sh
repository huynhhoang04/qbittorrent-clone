#!/usr/bin/env bash
# DEMO 6: TẢI TIẾP SAU KHI PEER BỊ CRASH
# Thao tác: Tắt cưỡng bức leecher đang tải, mở lại với cùng thư mục dữ liệu.
# PASS khi: File đúng SHA-256; lần mở lại chỉ tải một phần số chunk, không tải lại toàn bộ.
# Chạy: bash scripts/demo/06-crash-resume.sh
# Các tiến trình chạy trên một máy qua localhost; tự dọn sau khi kết thúc.

if [[ "${BASH_SOURCE[0]}" == "$0" ]]; then
  exec bash "$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)/demo_full.sh" 6 "$@"
fi

demo_title='TẢI TIẾP SAU KHI PEER BỊ CRASH'
demo_action='Tắt cưỡng bức leecher đang tải, mở lại với cùng thư mục dữ liệu.'
demo_expect='File đúng SHA-256; lần mở lại chỉ tải một phần số chunk, không tải lại toàn bộ.'
demo_function=scenario_crash_resume

scenario_crash_resume() {
  local port="$1" url="http://127.0.0.1:$1" root="$work/s6"
  mkdir -p "$root/leech" "$root/out"
  make_source "$root/file.bin" 4096 6
  local id expected; id="$(sha "$root/file.bin")"; expected="$id"

  local seeder
  seeder="$(start_peer seed "$root/seed.log" --tracker "$url" --port 0 \
    --data-dir "$root/seed" --seed "$root/file.bin" --chunk-size 32768 --chunk-delay-ms 80)"
  wait_for_file "$root/file.bin.torrent.json"

  local first_pid
  first_pid="$(start_peer leech1 "$root/leech1.log" --tracker "$url" --port 0 \
    --data-dir "$root/leech" --download "$root/file.bin.torrent.json" \
    --output-dir "$root/out" --chunk-size 32768 --exit-after-download)"

  # Only interrupt after the tracker has seen this leecher own some chunks.
  local partial
  partial="$(wait_for_partial "$port" "$id" "$root/leech1.log" "$first_pid")" || partial=""

  if [ -z "$partial" ]; then
    kill -9 "$first_pid" "$seeder" >/dev/null 2>&1 || true
    record 6 "Crash resume from persisted state" "FAIL: never observed partial progress to interrupt"
    return
  fi
  printf '[TIẾN ĐỘ] Leecher đã có %s chunk.\n' "$partial"
  printf '[GÂY LỖI] Tắt cưỡng bức leecher; mở lại với cùng dữ liệu để tải tiếp.\n'
  if ! bash "$script_dir/inject_fault.sh" "$first_pid" crash >"$root/fault.log" 2>&1; then
    record 6 "Crash resume from persisted state" "FAIL: could not crash the leecher during transfer"
    return
  fi

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

#!/usr/bin/env bash
# DEMO 4: MẤT NGUỒN KHI ĐANG TẢI
# Thao tác: Đợi leecher tải được một phần từ S1, mở S2 rồi cưỡng bức tắt S1.
# PASS khi: Leecher tiếp tục qua S2 và hoàn thành file đúng SHA-256.
# Chạy: bash scripts/demo/04-failover.sh
# Các tiến trình chạy trên một máy qua localhost; tự dọn sau khi kết thúc.

if [[ "${BASH_SOURCE[0]}" == "$0" ]]; then
  exec bash "$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)/demo_full.sh" 4 "$@"
fi

demo_title='MẤT NGUỒN KHI ĐANG TẢI'
demo_action='Đợi leecher tải được một phần từ S1, mở S2 rồi cưỡng bức tắt S1.'
demo_expect='Leecher tiếp tục qua S2 và hoàn thành file đúng SHA-256.'
demo_function=scenario_failover

scenario_failover() {
  local port="$1" url="http://127.0.0.1:$1" root="$work/s4"
  mkdir -p "$root/leech" "$root/out"
  make_source "$root/file.bin" 4096 4
  local id expected; id="$(sha "$root/file.bin")"; expected="$id"

  # Start with S1 alone, so partial progress proves it really served the file.
  local doomed healthy
  doomed="$(start_peer doomed "$root/doomed.log" --tracker "$url" --port 0 \
    --data-dir "$root/doomed" --seed "$root/file.bin" --chunk-size 32768 --chunk-delay-ms 120)"
  wait_for_file "$root/file.bin.torrent.json"

  local leech_pid
  leech_pid="$(start_peer leech "$root/leech.log" --tracker "$url" --port 0 \
    --data-dir "$root/leech" --download "$root/file.bin.torrent.json" \
    --output-dir "$root/out" --chunk-size 32768 --io-timeout-ms 3000 \
    --max-downloads 1 --pipeline-depth 1 \
    --exit-after-download)"
  local partial
  if ! partial="$(wait_for_partial "$port" "$id" "$root/leech.log" "$leech_pid")"; then
    record 4 "Seeder failure with failover" "FAIL: never observed partial progress from S1"
    return
  fi
  printf '[TIẾN ĐỘ] Leecher đã có %s chunk từ S1.\n' "$partial"
  healthy="$(start_peer healthy "$root/healthy.log" --tracker "$url" --port 0 \
    --data-dir "$root/healthy" --seed "$root/file.bin" --chunk-size 32768)"
  printf '[GÂY LỖI] Tắt cưỡng bức S1; leecher phải tải tiếp từ S2.\n'
  if ! bash "$script_dir/inject_fault.sh" "$doomed" crash >"$root/fault.log" 2>&1; then
    record 4 "Seeder failure with failover" "FAIL: could not crash S1 during transfer"
    return
  fi

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

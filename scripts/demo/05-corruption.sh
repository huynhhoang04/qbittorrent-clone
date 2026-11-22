#!/usr/bin/env bash
# DEMO 5: PHÁT HIỆN VÀ TẢI LẠI CHUNK HỎNG
# Thao tác: Mở 1 nguồn cố tình gửi chunk sai và 1 nguồn gửi đúng.
# PASS khi: Log có SHA-256 mismatch; file cuối cùng vẫn đúng SHA-256.
# Chạy: bash scripts/demo/05-corruption.sh
# Các tiến trình chạy trên một máy qua localhost; tự dọn sau khi kết thúc.

if [[ "${BASH_SOURCE[0]}" == "$0" ]]; then
  exec bash "$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)/demo_full.sh" 5 "$@"
fi

demo_title='PHÁT HIỆN VÀ TẢI LẠI CHUNK HỎNG'
demo_action='Mở 1 nguồn cố tình gửi chunk sai và 1 nguồn gửi đúng.'
demo_expect='Log có SHA-256 mismatch; file cuối cùng vẫn đúng SHA-256.'
demo_function=scenario_corruption

scenario_corruption() {
  local port="$1" url="http://127.0.0.1:$1" root="$work/s5"
  mkdir -p "$root/leech" "$root/out"
  make_source "$root/file.bin" 1024 5
  local id expected; id="$(sha "$root/file.bin")"; expected="$id"

  local poison honest
  poison="$(start_peer poison "$root/poison.log" --tracker "$url" --port 0 \
    --data-dir "$root/poison" --seed "$root/file.bin" --chunk-size 32768 \
    --corrupt-chunks 0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31)"
  honest="$(start_peer honest "$root/honest.log" --tracker "$url" --port 0 \
    --data-dir "$root/honest" --seed "$root/file.bin" --chunk-size 32768)"
  wait_for_file "$root/file.bin.torrent.json"
  for _ in $(seq 1 40); do
    seen="$(peers_json "$port" "$id")"
    count="$(echo "$seen" | grep -o '"peer_id"' | wc -l | tr -d ' ')"
    [ "${count:-0}" -ge 2 ] && break
    sleep 0.25
  done

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

#!/usr/bin/env bash
# DEMO 3: TẢI FILE TỪ NHIỀU NGUỒN
# Thao tác: Mở 2 seeder và 1 leecher; leecher tải song song.
# PASS khi: File đúng SHA-256 và ít nhất 2 nguồn thực sự gửi chunk.
# Chạy: bash scripts/demo/03-multi-source.sh
# Các tiến trình chạy trên một máy qua localhost; tự dọn sau khi kết thúc.

if [[ "${BASH_SOURCE[0]}" == "$0" ]]; then
  exec bash "$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)/demo_full.sh" 3 "$@"
fi

demo_title='TẢI FILE TỪ NHIỀU NGUỒN'
demo_action='Mở 2 seeder và 1 leecher; leecher tải song song.'
demo_expect='File đúng SHA-256 và ít nhất 2 nguồn thực sự gửi chunk.'
demo_function=scenario_multi_source

scenario_multi_source() {
  local port="$1" url="http://127.0.0.1:$1" root="$work/s3"
  mkdir -p "$root/leech" "$root/out"
  make_source "$root/file.bin" 1024 3
  local id expected; id="$(sha "$root/file.bin")"; expected="$id"

  # Both seeders hold the whole file and each may serve at most one peer, so a
  # completed transfer that reports two contributors really used both sources.
  local seed_a seed_b
  seed_a="$(start_peer seedA "$root/seedA.log" --tracker "$url" --port 0 \
    --data-dir "$root/seedA" --seed "$root/file.bin" --chunk-size 32768 --max-uploads 1 --chunk-delay-ms 10)"
  seed_b="$(start_peer seedB "$root/seedB.log" --tracker "$url" --port 0 \
    --data-dir "$root/seedB" --seed "$root/file.bin" --chunk-size 32768 --max-uploads 1 --chunk-delay-ms 10)"
  wait_for_file "$root/file.bin.torrent.json"
  for _ in $(seq 1 60); do
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

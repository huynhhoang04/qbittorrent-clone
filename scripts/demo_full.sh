#!/usr/bin/env bash
# DEMO DS02: run one of seven scenarios, or all of them (the default).
# Usage: bash scripts/demo_full.sh [1..7|all] [--keep-logs]
# Each scenario prints its purpose and PASS conditions before doing any work.
# Uses real local TrackerApp/PeerApp processes; builds must already exist.
set -uo pipefail

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
repo_root="${QBT_REPO_ROOT:-$(cd "$script_dir/.." && pwd)}"
build_dir="${QBT_BUILD_DIR:-$repo_root/build}"
scenario_files=(
  01-discovery.sh
  02-single-source.sh
  03-multi-source.sh
  04-failover.sh
  05-corruption.sh
  06-crash-resume.sh
  07-tracker-expiry.sh
)

usage() {
  cat <<'HELP'
DEMO DS02 — CHỌN ĐÚNG TÌNH HUỐNG CẦN TRÌNH BÀY

  bash scripts/demo_full.sh [1..7|all] [--keep-logs]
  Hoặc chạy trực tiếp một file trong scripts/demo/:

  1  01-discovery.sh       Đăng ký và tìm peer
  2  02-single-source.sh   Tải file từ một nguồn
  3  03-multi-source.sh    Tải file từ nhiều nguồn
  4  04-failover.sh        Mất nguồn khi đang tải
  5  05-corruption.sh      Phát hiện và tải lại chunk hỏng
  6  06-crash-resume.sh    Tải tiếp sau khi peer bị crash
  7  07-tracker-expiry.sh  Tracker loại peer mất liên lạc (~1 phút)

Không truyền số: chạy cả 7 bài. --list: xem danh sách. --keep-logs: giữ log.
Mỗi lần chạy tự mở tracker và peer riêng trên một máy qua localhost, không Web UI.
Chạy xong tự dừng các tiến trình đã mở. Khi FAIL sẽ giữ thư mục log để xem lỗi.
Chọn bản build: QBT_BUILD_DIR="$PWD/build-release" bash scripts/demo_full.sh 3
HELP
}

selection=all
selection_set=0
keep_logs=0
for arg in "$@"; do
  case "$arg" in
    -h|--help|--list) usage; exit 0 ;;
    --keep-logs) keep_logs=1 ;;
    all|[1-7])
      if [ "$selection_set" -eq 1 ]; then
        echo 'Chỉ chọn một số 1..7 hoặc all.' >&2
        exit 2
      fi
      selection="$arg"
      selection_set=1
      ;;
    *) echo "Tham số không hợp lệ: $arg (xem --help)." >&2; exit 2 ;;
  esac
done

resolve_bin() {
  if [ -f "$1" ]; then echo "$1";
  elif [ -f "$1.exe" ]; then echo "$1.exe";
  else echo "$1"; fi
}
peer_bin="$(resolve_bin "$build_dir/src/peer/PeerApp")"
tracker_bin="$(resolve_bin "$build_dir/src/tracker/TrackerApp")"
for binary in "$peer_bin" "$tracker_bin"; do
  if [ ! -f "$binary" ]; then
    echo "missing binary: $binary (build the project first or set QBT_BUILD_DIR)" >&2
    exit 1
  fi
done
for dependency in curl sha256sum; do
  if ! command -v "$dependency" >/dev/null 2>&1; then
    echo "missing command: $dependency" >&2
    exit 1
  fi
done

work="$(mktemp -d "${TMPDIR:-/tmp}/qbt_demo_XXXXXX")" || exit 1
work="$(cd "$work" && pwd -P)"
pids=()
results=()
pid_registry="$work/pids"
: >"$pid_registry"
cleanup() {
  local pid
  for pid in $(cat "$pid_registry" 2>/dev/null) "${pids[@]:-}"; do
    kill -9 "$pid" >/dev/null 2>&1 || true
  done
  wait >/dev/null 2>&1 || true
  if [ "$keep_logs" -eq 1 ]; then
    printf '\nLog và dữ liệu demo: %s\n' "$work"
  else
    rm -rf -- "$work"
  fi
}
trap cleanup EXIT
trap 'exit 130' INT
trap 'exit 143' TERM
source "$script_dir/demo/common.sh"

if [ "$selection" = all ]; then
  selected=(1 2 3 4 5 6 7)
else
  selected=("$selection")
fi
for number in "${selected[@]}"; do
  source "$script_dir/demo/${scenario_files[number-1]}"
  printf '\n============================================================\n'
  printf 'DEMO %s: %s\n' "$number" "$demo_title"
  printf 'THAO TÁC: %s\nPASS KHI: %s\n' "$demo_action" "$demo_expect"
  printf '============================================================\n'
  if [ "$number" = "${selected[0]}" ]; then
    printf 'Chạy trên localhost; tự mở và dọn các tiến trình demo.\n'
    printf 'Build: %s\nThư mục log: %s\n' "$build_dir" "$work"
    tracker_port="$(pick_port)" || { echo 'Không tìm được cổng trống.'; exit 1; }
    if ! start_tracker "$tracker_port" "$work/tracker.log"; then
      keep_logs=1
      echo "Tracker không khởi động được; xem $work/tracker.log" >&2
      exit 1
    fi
    printf 'Tracker: http://127.0.0.1:%s\n' "$tracker_port"
  fi
  previous_count=${#results[@]}
  "$demo_function" "$tracker_port"
  if [ "${#results[@]}" -ne "$((previous_count + 1))" ]; then
    record "$number" "$demo_title" 'FAIL: scenario did not produce exactly one result'
  fi
done

printf '\n================= demo results =================\n'
failed=0
for entry in "${results[@]}"; do
  number="${entry%%|*}"
  rest="${entry#*|}"
  name="${rest%%|*}"
  detail="${rest#*|}"
  printf 'DEMO %s  %s  %s\n' "$number" "$name" "$detail"
  if [[ "$detail" != PASS* ]]; then failed=$((failed + 1)); fi
done
if [ "$failed" -eq 0 ]; then
  if [ "$selection" = all ]; then echo 'ALL 7 DEMOS PASS';
  else printf 'DEMO %s PASS\n' "$selection"; fi
else
  printf '%s of %s demos FAILED\n' "$failed" "${#selected[@]}"
fi
exit "$failed"

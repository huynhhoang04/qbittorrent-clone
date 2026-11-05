#!/usr/bin/env bash
# Generate a random test file of the requested size.
#   usage: generate_test_file.sh <size_mb> <output_path>
set -euo pipefail

size_mb="${1:-10}"
output="${2:?usage: generate_test_file.sh <size_mb> <output_path>}"

case "$size_mb" in
  10|50|100) ;;
  *) ;;  # any positive size is accepted; the milestone names 10/50/100
esac

mkdir -p "$(dirname "$output")"
dd if=/dev/urandom of="$output" bs=1048576 count="$size_mb" status=none
printf 'generated %s (%s MB) sha256=%s\n' "$output" "$size_mb" \
  "$(sha256sum "$output" | cut -d' ' -f1)"

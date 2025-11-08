#!/bin/bash
REAL_RUSTC="$1"
shift
rustc_out_dir=""
prev=""
for arg in "$@"; do
  if [ "$prev" = "--out-dir" ]; then
    rustc_out_dir="$arg"
    break
  fi
  prev="$arg"
done
if [ -n "$rustc_out_dir" ]; then
  mkdir -p "$rustc_out_dir"
  export TMPDIR="$rustc_out_dir"
  export RUSTC_TMPDIR="$rustc_out_dir"
fi
exec "$REAL_RUSTC" "$@"

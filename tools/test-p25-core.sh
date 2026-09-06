#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$repo_root"
build_dir="$(mktemp -d)"
trap 'rm -rf "$build_dir"' EXIT

for source in components/mbelib/mbelib.c components/mbelib/imbe7200x4400.c components/mbelib/ecc.c; do
  name="$(basename "$source" .c)"
  cc -std=gnu11 -O2 -w -Icomponents/mbelib -c "$source" -o "$build_dir/$name.o"
  cc -std=gnu11 -O1 -g -w -fsanitize=address,undefined -fno-omit-frame-pointer \
    -Icomponents/mbelib -c "$source" -o "$build_dir/$name.san.o"
done

sources=(tests/p25_core_tests.cpp apps/orcsdr-tab5/ui/p25_decoder_core.cpp apps/orcsdr-tab5/ui/p25_voice.cpp)
objects=("$build_dir/mbelib.o" "$build_dir/imbe7200x4400.o" "$build_dir/ecc.o")
san_objects=("$build_dir/mbelib.san.o" "$build_dir/imbe7200x4400.san.o" "$build_dir/ecc.san.o")
common=(-std=c++17 -Wall -Wextra -Werror -pedantic -Iapps/orcsdr-tab5/ui -Icomponents/mbelib)

g++ "${common[@]}" -O2 "${sources[@]}" "${objects[@]}" -lm -o "$build_dir/p25_core_tests"
"$build_dir/p25_core_tests" tests/fixtures/p25_control_lane_453925.orciq

g++ "${common[@]}" -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer \
  "${sources[@]}" "${san_objects[@]}" -lm -o "$build_dir/p25_core_tests_sanitized"
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 \
  "$build_dir/p25_core_tests_sanitized" tests/fixtures/p25_control_lane_453925.orciq

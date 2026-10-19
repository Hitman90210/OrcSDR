#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
out=$(mktemp -d)
trap 'rm -rf "$out"' EXIT
ui=apps/orcsdr-tab5/ui
flags=(-std=c++17 -O1 -g -Wall -Wextra -Werror -fsanitize=address,undefined -fno-sanitize-recover=all -ffunction-sections -fdata-sections -Wl,--gc-sections -I"$ui")
run() {
  local name=$1; shift
  g++ "${flags[@]}" "$@" -o "$out/$name"
  "$out/$name"
  echo "$name PASS"
}
run home_cadence tests/home_cadence_tests.cpp
run waterfall_style tests/waterfall_style_tests.cpp "$ui/waterfall_style.cpp"
run lora_history tests/lora_history_tests.cpp "$ui/lora_history.cpp" "$ui/lora_packet_log.cpp"

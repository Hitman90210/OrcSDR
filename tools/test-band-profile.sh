#!/usr/bin/env bash
# The band table, compiled and run on the host.
#
# tests/band_profile_tests.cpp exists but nothing ran it: no workflow and no
# script referenced it, so the only check the channel plan ever got was
# self_check() on a booted device. That matters for the GMRS work -- a
# channelized profile has to sit on its raster and must not overlap a named
# neighbour, and self_check() is what enforces both.
#
# The OrcDial name table is vendored at orcdial/src, so the static_assert that
# ties orc::band_name_count to kProfileCount is checkable here, in one repo.
set -euo pipefail
cd "$(dirname "$0")/.."
out=$(mktemp -d)
trap 'rm -rf "$out"' EXIT
ui=apps/orcsdr-tab5/ui
flags=(-std=c++17 -O1 -g -Wall -Wextra -Werror -fsanitize=address,undefined -fno-sanitize-recover=all -ffunction-sections -fdata-sections -Wl,--gc-sections -I"$ui" -Iorcdial/src)
run() {
  local name=$1; shift
  g++ "${flags[@]}" "$@" -o "$out/$name"
  "$out/$name"
  echo "$name PASS"
}
run band_profile tests/band_profile_tests.cpp "$ui/band_profile.cpp"

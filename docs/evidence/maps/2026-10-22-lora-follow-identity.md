# LoRa Follow identity regression

The history moves recently heard nodes to row zero, while the dashboard's
selected row is an index. Following that row on every snapshot could silently
switch the map to a different device. A focused regression swaps two real-shaped
node rows during Follow and fails against the original implementation.

The correction stores the followed device ID at Follow activation. Camera,
sidebar and marker emphasis resolve that ID on subsequent snapshots. If absent
or unmappable, retain the camera and show Waiting Position. Pan/Receiver/Node
Center disable Follow; re-entry resets it. Existing selected-row behavior outside
Follow is unchanged. Serial status exposes the followed ID without changing
authentication or the shared action handler.

Changed: `lora_dashboard.cpp/.hpp`, `tests/lora_map_controls_tests.cpp`,
`docs/MAP_SERIAL_COMMANDS.md` and this note. No NVS, SD or decoder changes.

From the integration worktree root, commands executed:

```text
wsl -d Ubuntu -- bash -lc 'cd /mnt/f/AI/OrcSDR-TEMP/maps-integration-completion && cmake --build .local/maps/labels-host --target orcsdr_lora_map_tests -j2 && ctest --test-dir .local/maps/labels-host -R "^lora_map_controls$" --output-on-failure'
wsl -d Ubuntu -- bash -lc 'cd /mnt/f/AI/OrcSDR-TEMP/maps-integration-completion && cmake --build .local/maps/map-serial-asan-20261021 --target orcsdr_lora_map_tests -j2 && ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 .local/maps/map-serial-asan-20261021/orcsdr_lora_map_tests'
python .local/maps/check_lora_map_syntax_20261021.py
```

Red assertion: `lora-follow-identity-red-20261022.log`. Final normal 1/1 PASS,
ASan/UBSan PASS, P4 syntax for dashboard/adapter/main PASS:
`lora-follow-identity-{host,asan,p4-syntax}-20261022.log`. Tests cover row reorder,
same-ID movement, disappearance/reappearance, lost position, status ID and
existing pan/zoom/ownership/authentication behavior.

Not yet native built or installed; no hardware operation for this correction.
The currently installed COM17 app remains `7c48e358`. Native combined validation
is deferred to the separately authorized TEST-ONLY download-pilot candidate.

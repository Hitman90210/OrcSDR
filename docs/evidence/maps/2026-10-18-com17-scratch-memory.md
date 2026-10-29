# COM17 fresh-boot Pair refusal: decoder scratch placement

Source 329e5a9e; native build and COM17 fresh-boot Pair-start checks passed.
OrcDial matching-code exchange and connected status passed; live decoder scratch exercise remains pending.

The user reproduced insufficient memory to pair after rebooting COM17 on P4
source 6f001d86. The read-only capture `com17-pair-failure-20261018.log` confirms
DMA free 3,671, largest 3,200, bridge=0/failure=low_memory, and no held Map frame.
COM17 keeps C6 3.0.6; COM39 must retain C6 1.4.1. No C6 change is authorized
by this fix.

The existing ELF places `ft8_runtime::decode_slot(...)::chunk`, an 8,192-byte
int16 PCM staging array, at 0x4ff61dec in internal RAM. It is reserved at boot
even with FT8 inactive. Code inspection shows it is used only while the decoder
task copies samples from the existing PSRAM ring into the FT8/FT4/JS8 backend.
It is not an ISR or hardware DMA buffer. This is a directly identified avoidable
internal allocation, not an assertion that it explains every memory difference
between the two devices.

The narrow fix marks this existing zero-initialized static array
`EXT_RAM_BSS_ATTR`. It keeps the 4,096-sample size, copying, decoder algorithms,
task stacks, thread ownership, driver pin, C6 and pairing admission thresholds.
It does not add a buffer pool or change receiver/USB lifecycle.

`tools/check_tab5_ft8_scratch.py` checks the actual linked symbol's size and
containment in `.ext_ram.bss`, and is registered as an ALL target in native CMake
so an internal placement regression fails the build.

Tests run before native build:

- Placement gate on the existing ELF: expected FAIL, 8,192 bytes at 0x4ff61dec.
  Log `com17-scratch-placement-red-20261018.log`.
- Real P4 syntax check of ft8_runtime.cpp: PASS.
  Log `com17-scratch-p4-syntax-20261018.log`.
- WSL `bash tools/test-ft8-diagnostic.sh`: PASS for production, diagnostic and
  ASan/UBSan backend/pipeline variants and diagnostic argument handling.
  Log `com17-scratch-ft8-diagnostic-20261018.log`.

Next milestone: native link/placement gate, fresh COM17 recovery checkpoint,
app-only flash, measured post-boot DMA/RTC memory and Pair while receiving.
Preserve the verified original October 15 COM17 full-flash snapshot and the
current 6f001d86 recovery option. Then exercise FT8/JS8 scratch use and SD
readback; do not generalize a host pass into hardware/performance acceptance.

## Native build and COM17 result

Native command: `./apps/orcsdr-tab5/tools/build-tab5-idf.ps1 -WorldMap F:/AI/OrcSDR-TEMP/maps-integration-completion/.local/maps/world/world-overview-z4.pmtiles`.
Result: PASS, including miniz symbol verification and the new placement gate:
8,192 bytes at 0x484e7440 in `.ext_ram.bss`.
Log: `com17-scratch-native-build-20261018.log`.

Candidate app SHA256:
`503d667c7422efb25edeb9d78799f5eeebc9226e93769b5ad0cb79560c618900`.
ELF SHA256:
`41ec97f0a6671e7cb83c1960e13db082c85b5b71f3617b9044bc2462158243bf`.
App size 5,029,568 bytes; full 40,960-byte internal reserve retained.

COM17 identity 30:ED:A0:E2:E6:95 verified before app-only flash. Fresh recovery
checkpoint: `.local/maps/com17-recovery/pcm-scratch-20261018/manifest.json`.
Previous app verification, flash verification, and preservation checks all PASS.
Logs: `com17-scratch-verify-previous-20261018.log`,
`com17-scratch-flash-20261018.log`,
`com17-scratch-preservation-20261018.log`,
`com17-scratch-candidate-verify-20261018.log`.
NVS, SD files, boot/table/PHY/core/world and C6 were preserved. COM39 untouched.

Controlled boot capture reached ready with SD/Wi-Fi/RTL all ready:
`com17-scratch-boot-20261018.log`.
Read-only health capture: `com17-scratch-health-retry-20261018.log`.
With Blog V4 receiving, DMA free=12,827, largest=12,288; bridge initially off.
Prior failing capture had free=3,671, largest=3,200. These are measured snapshots,
not a claim of identical allocation timing. C6 remains host/cop 3.0.6, match=1.
An initial health attempt hit the still-open startup monitor and sent no commands;
its empty log is retained as `com17-scratch-health-20261018.log`.

Authenticated `RTL_ORCDIAL_PAIR START` now starts the bridge: bridge=1,
connection=Searching, failure=None. After start DMA free=10,191, largest=9,728,
secure-task minimum free stack=9,224. Log: `com17-scratch-pair-20261018.log`.
Original tune restored with `RTL_FREQ 10135000`. During the bounded capture,
HOME reported 23-24 updates/s and zero IQ/audio drops. Pair admission is verified;
actual secure pairing, controls, sustained operation and live FT8 scratch exercise
are not yet established by this capture. Next milestone: finish the peer handshake
and exercise the moved buffer during actual decoder slot processing.

## Completed pairing

User confirmed both codes matched and OrcDial connected. A subsequent read-only
COM17 capture independently reports `bridge=1 trust=1 connection=Connected
boot=1 failure=None protocol=4`. Command:
`python .local/maps/com39_bench.py .local/maps/com17-pair-failure-plan.json com17-scratch-connected-20261018.log --port COM17`.
Result: PASS; no reset, flash or pairing-state changes in this observation.
DMA free=10,191, largest=9,728; observed DMA minimum=9,527; secure task minimum
free stack=8,568. C6 host/cop remains 3.0.6. Pairing after the captured fresh boot
is verified. Dial control operations, reconnect after another reboot, sustained
operation, live FT8 scratch use and SD concurrency remain separate gates.

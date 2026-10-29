# COM17 map/dashboard checkpoint — October 22, 2026

Source candidate: `050442b0`, including ADS-B range correction `19856eea`.
Current source also contains HTTP-adapter tests `15f1f0bf`; these tests do not
change the installed firmware. Device: COM17, USB serial `30:ED:A0:E2:E6:95`.
COM39 was not used. This record supersedes the earlier pending-port status.

## Installed artifact and preservation

Artifact directory: `.local/maps/lora-map-controls-050442b0-20261021/`.
App size: 5,139,024 bytes; factory headroom: 1,152,432 bytes.
App SHA-256: `43f431100b0e44c80b4e9b108f1543956e27e7ef5a04a74e7c8ae52428776565`.
ELF SHA-256: `ab18583d9848be536da9c1cf23d17210aee641800c0a424d3caa58e9ff224a32`.
The app-only flash and saved non-app preservation checks pass. NVS, embedded
world, C6, bootloader and partition table were preserved; SD packs were not
replaced. The 40 KiB internal DMA reserve is unchanged. No occupied-port process
was killed; the port became available before the update.

Invocation from the integration worktree root:

```text
python .local/maps/flash_lora_map_controls_20261021.py
python .local/maps/com17_label_bench.py lora-map-controls-device-plan-20261021.json lora-map-controls-device-20261021.log
python .local/maps/com17_label_bench.py adsb-map-range-device-plan-20261021.json adsb-map-range-device-20261021.log
python .local/maps/com17_label_bench.py lora050442b0-retained-sd-plan-20261022.json lora050442b0-retained-sd-device-20261022.log
```

Flash logs: root `lora-map-controls-verify-previous-20261021.log`,
`lora-map-controls-app-write-20261021.log` and
`lora-map-controls-preservation-20261021.log`, plus separate backup logs.
Native startup capture is in
`apps/orcsdr-tab5/lora-map-controls-boot-native-monitor-20261021.log`.
Log names retain their original invocation dates.

## Bounded results

- LoRa plan exits 0: pan, zoom in/out, receiver recenter and saved-coordinate
  preservation pass. Follow-on rejects a selected node without a usable
  position; follow-off passes. This does not test positioned-node following.
  Final streaming rate is 954,869 samples/s with zero observed driver drops or
  overruns. Free PSRAM is 4,128,356 bytes; largest DMA block is 8,192 bytes.
- ADS-B plan exits 0: dedicated range command, existing Settings action and
  routed +/− touches agree; malformed/out-of-range values are rejected.
  Original 100 NM radius is restored, with persistence confirmed after recovery
  reboot. This does not establish aircraft-overlay or RF decoding acceptance.
- Cached preview has only two samples: same-scale paint p50/p95 is
  25,999/26,785 microseconds; scaled paint is 28,793/67,197 microseconds.
  Dwell-inclusive update intervals are not FPS. Physical touch latency was not
  measured. The representative 33 ms gate remains open.
- SD substeps pass: uniquely named absent-checked 65,536-byte scratch file
  `/orcsdr/maplabels_330606cb_lora050442b0_20261022.bin` writes and reads back
  exactly. SHA-256 is
  `f8a38b49873588eea0fa55c178b2ad20a05eb7f3eccf55623323405fdec79fe9`.
  Map memory 751,376 bytes and LoRa IQ memory 15,840,000 bytes remain retained.
  Free PSRAM before/after is 4,079,552 bytes; largest DMA block is 8,192 bytes.
  Reception was stopped for transfer. The scratch file remains on SD.

## FT8 transition failure and recovery

The SD plan **exits 1** after the passing SD substeps. Opening FT8 starts its
runtime and live drawing; during subsequent serial authentication/snapshot
output, the device reports 416,052 bytes free and panics with a stack-protection
fault. This follows a long elapsed interval, not a controlled overnight soak.

Reported task: `main`; SP `0x4ff4c7e0`, bounds
`0x4ff4bfb0`–`0x4ff4c7e0`; MEPC `0x4ff002c0`. Address resolution identifies the
SDK interrupt handler. The native main task is configured for 16 KiB; the
reported interrupt-context bounds are not proof of ordinary main-task overflow.
Neither low-memory causation nor the faulting application call is established.

Read-only dump extraction (verified device identity first):

```text
python -m esptool --chip esp32p4 --port COM17 --baud 921600 --before default_reset --after no_reset read_flash 0x610000 0x40000 .local/maps/lora-ft8-stack-crash-20261022/coredump.bin
```

Saved partition is 262,144 bytes, with a 63,296-byte dump. SHA-256:
`dbba16e20e8640c335ce8fc4af24a4082f26a1a8c2ef867542fb3726f7889bc2`.
Evidence: `lora-ft8-coredump-read-20261022.log`,
`lora-ft8-coredump-convert-20261022.log`,
`lora-ft8-coredump-decoded-20261022.log`,
`lora-ft8-coredump-decoded-gdb16-20261022.log` and
`lora-ft8-coredump-gdb-backtraces-20261022.log` in the worktree root.
SDK decoders fail with `KeyError: current-thread-id`; manual GDB identifies
interrupt context but cannot unwind the faulting application chain. Raw dump
artifacts remain local and are not committed.

After controlled recovery, this command exits 0:

```text
python .local/maps/com17_label_bench.py lora-ft8-recovery-plan-20261022.json lora-ft8-recovery-device-20261022.log
```

Startup capture:
`apps/orcsdr-tab5/lora-ft8-recovery-boot-native-monitor-20261022.log`.
FT8 is restored at 7.070 MHz, streaming about 234,478 samples/s with zero observed
driver drops/overruns. Free PSRAM is 18,472,120 bytes, largest block 18,350,080;
largest DMA block 8,192; main stack high-water mark 1,896. Saved receiver
coordinates and ADS-B radius remain unchanged. Exact previous gain/AGC values
were not restored: final AUTO gain is 0 and RTL AGC is off. These are last logged
observations, not a claim about subsequent live device state.

## HTTP adapter host gate

`15f1f0bf` adds tests of production `ui/map_http_stream.cpp` against fake SDK
calls: TLS configuration, resume headers, redirects, bounded reads, rejection
and cleanup. Normal and ASan/UBSan executions both exit 0 with
`MAP_HTTP_STREAM_PASS`; no network is used.

```text
cmake -S apps/orcsdr-tab5/tests/maps -B .local/maps/labels-host
cmake --build .local/maps/labels-host --target orcsdr_map_http_stream_tests -j2
.local/maps/labels-host/orcsdr_map_http_stream_tests
cmake -S apps/orcsdr-tab5/tests/maps -B .local/maps/map-serial-asan-20261021
cmake --build .local/maps/map-serial-asan-20261021 --target orcsdr_map_http_stream_tests -j2
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 .local/maps/map-serial-asan-20261021/orcsdr_map_http_stream_tests
```

Commands ran through WSL from the worktree root. Root logs are
`map-http-stream-{host,sanitizer}-{configure,build,test}-20261021.log`.
Real HTTPS transfer and device installation/recovery remain unaccepted.

## Next gate

Investigate the FT8 transition using the preserved dump and allocation evidence;
do not assume a stack-size change fixes it. Full global-pack device quality,
positioned overlays/follow, setup recovery, actual HTTPS delivery and sustained
RF/performance acceptance remain open. No main merge or publication occurred.

## Follow-up: interrupted task context recovered

The matching ELF places `xIsrStack` at `0x4ff4bfb0`, the reported lower bound;
the configured ISR stack is 2,096 bytes. The original converted core lacks useful
values for some scheduler globals. Direct inspection of the saved main TCB at
`0x4ff31558` instead recovers `pxTopOfStack=0x4ff2d190`,
`pxStack=0x4ff2d1d8`, and `pxEndOfStack=0x4ff31350`. Its saved exception frame
contains PC `0x481596f4`, RA `0x481596d8`, and pre-interrupt SP `0x4ff2d210`:
only 56 bytes above the main stack's lower bound, with the interrupt frame
extending below that bound.

An explicitly derived core substitutes the saved main-task register frame into
the first PRSTATUS note; the original dump and converted core remain unchanged.
Reconstruction script: `.local/maps/reconstruct_interrupted_main_20261022.py`;
derived artifact: `.local/maps/lora-ft8-stack-crash-20261022/interrupted-main.elf`.
The script asserts both original and replacement PC/RA/SP before writing a new
file and refuses overwrite. This is offline reconstruction, not a new crash.

GDB then unwinds the main task through framebuffer `writeImage`, `pushImage`,
FT8 `scroll_waterfall`, `paced_waterfall_step`, `update`,
`request_hot_retune_for`, `process_command`, `poll_serial`, `loop`, and `app_main`.
This establishes that the failure occurred during synchronous FT8 drawing from
the serial retune path; interleaved serial snapshot output did not identify the
faulting call. Saved logs in the worktree root:

- `lora-ft8-coredump-stack-context-20261022.log` (missing scheduler globals).
- `lora-ft8-coredump-task-context-20261022.log` (direct saved TCB/frame fields).
- `lora-ft8-coredump-reconstructed-main-20261022.log` (reconstructed backtrace).
- `lora-ft8-coredump-main-frame-sizes-20261022.log` (frame addresses).

The installed ELF reserves 6,928 stack bytes in FT8 `update`, including its
6,840-byte automatic `Snapshot previous` copy. The prepared correction stores
that single-owner comparison snapshot in PSRAM and leaves the task stack and
DMA reserve unchanged. No managed component or receiver/DSP code is edited.

Focused checks:

```text
wsl bash -lc 'cd /mnt/f/AI/OrcSDR-TEMP/maps-integration-completion && g++ -std=c++17 -Wall -Wextra -Werror -pedantic -O2 -Iapps/orcsdr-tab5/ui -Itests/ft8_ui_stubs tests/ft8_dashboard_tests.cpp apps/orcsdr-tab5/ui/{ft8_dashboard,ft8_model,ft8_conditions,ft8_hunter,ft8_tuning,ft8_info_text,focus_nav}.cpp -o .local/maps/ft8-stack-dashboard-20261022 && .local/maps/ft8-stack-dashboard-20261022 > ft8-stack-dashboard-test-20261022.log 2>&1'
python .local/maps/check_ft8_stack_20261022.py
```

The dashboard test exits 0, `ft8_dashboard_tests: PASS`; repeating with
`-O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer` and
`ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1` also
exits 0 (`ft8-stack-dashboard-asan-test-20261022.log`). The P4 compile reuses the
native compile database, adds `-fstack-usage`, and asserts the update frame is
below 1,024 bytes: actual **80 bytes, static**. Compile log:
`ft8-stack-p4-compile-20261022.log`; usage file:
`.local/maps/ft8-stack-20261022.su`. P4 object symbol inspection confirms
`g_previous_snapshot` is 6,840 bytes in `.ext_ram.bss.0`.

These results support a narrow stack-pressure correction. Native linking,
final placement and COM17 regression of the captured transition are the next
gates; the crash is not yet closed by a passing device test.

## Installed correction and bounded regression

Correction commit: `89109160`. Native build command:

```text
python .local/maps/build_fit_order_20261020.py build ft8-stack-20261022
C:/Espressif/python_env/idf5.5_py3.14_env/Scripts/python.exe .local/maps/inspect_ft8_stack_20261022.py
python .local/maps/com17_label_bench.py ft8-stack-preflash-plan-20261022.json ft8-stack-preflash-device-20261022.log
python .local/maps/flash_ft8_stack_20261022.py
```

Build, artifact inspection, verified reception stop, app-only flash and fresh
non-app backup/preservation checks all pass. The app remains 5,139,024 bytes,
with 1,152,432 bytes factory headroom. Final ELF places the 6,840-byte comparison
snapshot at `0x48509440` in `.ext_ram.bss`. DMA reserve remains 40 KiB; existing
C6 input, diagnostics OFF and symbol/dependency gates pass.
App SHA-256: `3e312c74341078054d2852c908aa9afbe0e0ae75476c17de0aca739cf9cfeeb8`.
ELF SHA-256: `d11d2de1a2a904ebb3608899d6f1afb4c3e6393631788285e4c205171b341d97`.
Artifact: `.local/maps/ft8-stack-89109160-20261022/`, including fresh non-app
backups. Rollback app is the previously verified `050442b0` artifact.

Build log: `fit-order-native-ft8-stack-20261022-20261020.log` (the wrapper retains
its historical filename suffix). Inspection: `ft8-stack-artifact-inspection-20261022.log`.
Flash logs: root `ft8-stack-{verify-previous,backup-*,verify-backups,app-write,preservation}-20261022.log`.

The first native monitor requested the approved boot reset, then failed with a
host-side `Logger.pc_address_decoder` AttributeError when address decoding was
disabled. It is terminal, not a firmware crash. A subsequent normal-decoding
monitor attaches using `--no-reset` and records SD/Wi-Fi/RTL ready. Both captures
are preserved in `apps/orcsdr-tab5/ft8-stack-{boot,running}-native-monitor-20261022.log`.
The second monitor was closed before command testing.

```text
python .local/maps/com17_label_bench.py ft8-stack-regression-plan-20261022.json ft8-stack-regression-device-20261022.log
python .local/maps/com17_label_bench.py ft8-stack-final-state-current-plan-20261022.json ft8-stack-final-state-current-device-20261022.log
```

Bounded retune substeps **PASS**: LoRa/map visits retain 15,840,000 bytes of IQ
buffers and a 751,376-byte map frame; FT8 Live opens, then five serial retunes
between 7.070 and 7.074 MHz acknowledge successfully. Waterfall updates continue
at 72–73 rows per five-second report; no panic or reset is recorded. Six health
samples have monotonic uptime. FT8 streaming reports 237,829 samples/s with
zero observed driver drops/overruns. Free PSRAM is about 409,512 bytes under
this pressure; minimum reported main-stack margin is 1,544 bytes.

The original **whole regression plan exits 1** on a later stale expectation of
100 NM ADS-B range: actual current setting is 25 NM. An initial final-state
check also exits 1 because it expects historical receiver coordinates. The
current coordinates `440628866,-1230160273` match the earlier reading in this
same run; flash verification confirms unchanged NVS. Neither current range nor
location is overwritten to satisfy the old assertions. These test-assumption
failures are distinct from the passing retune substeps. The corrected read-only
final-state plan exits 0. Evidence extraction checks five retune acknowledgments,
retained allocations, live waterfall and monotonic uptime in
`ft8-stack-retune-evidence-check-20261022.log`.

Final observed state: ADS-B at 1090 MHz, 25 NM radius, streaming about 2,047,866
samples/s, zero observed drops/overruns. Free PSRAM is 483,604 bytes; largest
PSRAM block 319,488; largest DMA block 7,936; main-stack margin 1,544; uptime
190,877 ms, reset reason unchanged from the controlled update. Gain is AUTO,
280 tenths dB, RTL AGC off after the LoRa guard workload; exact pretest gain
was not restored. All serial sessions are closed.

The captured stack-pressure path has a source correction and passing bounded
device regression. This is not a soak, proof against every stack fault, or
acceptance of the tight retained-memory budget. General low-memory handling,
full dashboard/RF acceptance, global-pack delivery and performance gates remain
open. No additional SD transfer was performed for this source-only drawing fix.

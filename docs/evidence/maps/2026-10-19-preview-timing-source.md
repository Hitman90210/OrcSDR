# Preview feedback recorder — October 19, 2026

## Scope

Parent OrcSDR commit: 04b118a4. Source files: map_service.cpp, map_timing.hpp
and map_timing_tests.cpp. Preserved wizard/navigation edits remain separate.
No device access, native link, full suite, dependency update, push or merge.

The old recorder measures worker frames, not cached motion. The new recorder
keeps 256 samples per metric in two PSRAM-declared pools; a host static assertion
bounds both pools to 13 KiB. Actual linked P4 placement is still unverified.
The camera request gains one 64-bit acceptance timestamp; DMA reserve unchanged.

Only the first preview of each camera request contributes a sample in a bucket.
Its timestamp and identity survive repaint/nonblocking preview-lock retries.
Reads/writes use the existing preview mutex. Reports precede worker-lock access,
so a busy decoder does not suppress preview diagnostics. No blocking UI lock
is added; missed preview-lock attempts are counted.

## Metrics and limits

ORC_MAP_PREVIEW reports mode=same_scale or scaled, with:

- paint_call: time around the preview paint call.
- request_feedback: accepted draw request to its first preview paint completion,
  including intervening preview-lock retries for that request.
- present_interval: intervals between distinct preview requests, explicitly
  raw_includes_dwell=1. Use a controlled continuous gesture when interpreting it.

All report physical_touch_latency=0. Acceptance starts in software, not at touch
sampling; completion is the paint call, not panel scanout. Buckets classify the
transform against the cached source, not gesture intent: a pan following a pending
zoom can be scaled. Completed worker frames retain separate metrics. Preview-only
intervals do not describe every visible worker-frame update. No 33 ms claim.

## Executed checks

Commands ran from F:/AI/OrcSDR-TEMP/maps-integration-completion; logs are there.

Configure, PASS:

    cmake -S apps/orcsdr-tab5/tests/maps -B .local/maps/timing-host-20261019 -G "Visual Studio 18 2026" -DORCMAPS_ROOT=F:/AI/OrcSDR-TEMP/orcmaps-quality-delivery

Log: preview-timing-configure-20261019.log.

Focused build and test:

    cmake --build .local/maps/timing-host-20261019 --config Release --target orcsdr_map_timing_tests
    ctest --test-dir .local/maps/timing-host-20261019 -C Release -R "^map_timing$" --output-on-failure

Before implementation, expected compile failure because PreviewTiming was absent:
preview-timing-red-20261019.log. This is test-dependency evidence, not a hardware
lag reproduction. After implementation: PASS, 1/1, preview-timing-build-20261019.log
and preview-timing-tests-20261019.log. After review-requested additions: PASS,
1/1, preview-timing-reviewed-build-20261019.log and
preview-timing-reviewed-tests-20261019.log. Covers duplicate exclusion, retained
request delay, invalid clocks, nonuniform percentiles, ring and uint32 rollover.

P4 syntax-only check, PASS:

    C:/Espressif/python_env/idf5.5_py3.14_env/Scripts/python.exe .local/maps/check_preview_timing_syntax_20261019.py

Log: preview-timing-p4-syntax-20261019.log. Existing ESP-IDF literal-suffix warning
remains. This preceded the diagnostic bucket string rename; final native
validation remains required. Syntax does not prove linked PSRAM placement.

Bounded read-only subagent review found no correctness blocker. Its naming
caveat and extra test suggestions were addressed; no whole-branch review claimed.

## Next gate

Scoped approval for one explicit-input native build, configuration/symbol/PSRAM
inspection and COM17 app-only test after identity/recovery verification. Measure
controlled cached movement under reception, memory, retained-map SD and radio
counters. COM39 belongs to Claude for Dead Air; do not touch it. No 33 ms claim.

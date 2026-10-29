# Release review and IQ continuity completion ledger

PR: #211, `codex/maps-integration-completion`. Starting local commit: `3e213113`.
User authorizes autonomous fixes, tests, builds, app-only hardware updates and
merge **after** the goal passes. Preserve both tablets' SD/NVS/world data and C6
firmware (COM17 3.0.6, COM39 1.4.1); leave unrelated worktrees alone.

## Required completion gates

- Resolve the still-valid findings in `2026-10-25-pr211-review-triage.md`,
  including the newer setup/documentation early-return finding.
- Restore CodeRabbit coverage for tests and shipping documentation; verify
  findings against current code rather than treating a green status as approval.
- Build and test the final reviewed source on hardware. The installed `9e3f0378`
  does not include the later `271dca27` LoRa/FT8/storage review fixes.
- Apply the October 26 owner decision: small, transient map-movement/detail-refill
  IQ drops are accepted for this release. Preserve the measurements and disclose
  the limitation; idle/stationary losses and continuing loss after settling remain
  blockers. The earlier zero-loss criterion below is historical, not the current gate.
- Keep release-matrix FAIL/BLOCKED rows visible; no unsupported relabeling.
- Merge only after these gates pass; verify the merge on origin/main. Validate
  the tagged package before publication. Intended tag: `v0.5.0-beta.1`.

## Task ledger

| Task | State / evidence |
| --- | --- |
| Setup pause ownership on external navigation and documentation early return | Source correction; `python tests/setup_navigation_pause_test.py` passed after reproducing missing cleanup; device validation pending |
| Location lookup WithCaps task lifetime | Source correction; `python tests/location_task_lifetime_test.py` passed after reproducing missing reaper; P4/device checks pending |
| OrcDial initialization allocation-failure backoff | Focused production-block test passed for both queues and TX task; P4 syntax passed; device pending |
| ADS-B bounded zoom redraw and range-limit no-op | Focused production-handler test passed; map panel only, no-op at both limits; P4 syntax passed |
| FT8 row transfer and pending-frame redraw | Sanitized real-dashboard test passed with every output pixel compared against source projection; locked pending frames do not repaint static panels; P4 syntax passed |
| Shared download notice/header geometry | FT8 title and FT8/LoRa dividers moved beyond the shared notice; compile-time geometry guards; notice test passed after correcting stale visible-string assertions |
| Download/render scheduling and shared SD ownership | Pending |
| Current SD capacity and LoRa label behavior device checks | Pending |
| Restore review scope; review tests and release notes | Temporary source-only filters removed; external full review pending |
| Map-interaction IQ loss | Owner accepted small transient movement/detail-refill drops on October 26; not a zero-loss pass. Preserve evidence and confirm idle/stationary recovery. |
| Native build and COM17/COM39 final source validation | Pending |
| Final review, CI, merge/main verification and package validation | Pending |

Ruling: retain the user's quick map-file check and background-download Back
behavior. Do not reinstate full archive hashing or turn Back into cancellation
to satisfy superseded reviewer assumptions.

Ruling: use an externally reaped, suspended location lookup task. ESP-IDF 5.5.4
documents that plain `vTaskDelete` leaks WithCaps allocations, while self-delete
with `vTaskDeleteWithCaps` allocates another task and can abort on allocation
failure. The UI task can reclaim completed work without that allocation.

Raw logs/artifacts remain outside Git beside their invocation. Every subsequent
hardware result must identify the actual app hash, receiver and configuration.

## 2026-10-26 focused verification

- `bash tools/test-tab5-cadence-history.sh`: seven groups passed, including four
  new lifecycle/zoom tests; log `pr211-lifecycle-regressions-20261026b.log`.
- Sanitized production FT8 dashboard test: passed; log
  `pr211-ft8-followups-sanitized-20261026b.log`.
- `ctest --test-dir .local/maps/feature-replay-integration-20261023 -R
  "^map_download_notice$" --output-on-failure`: 1/1 passed; log
  `pr211-notice-regression-20261026.log`.
- Exact P4 compile-command syntax checks: main, map screen, location lookup,
  FT8, ADS-B, LoRa and download notice passed; log
  `pr211-release-followups-p4-syntax-final-20261026.log`.
- COM17 read-only baseline: USB identity verified, V3c driver 0.9.3 streaming
  2.4 MSPS, lifetime driver overruns/drops zero; baseline is not a map-load
  continuity result. Log `pr211-release-baseline-com17-20261026.log`.

The notice regression initially failed because the newer display stub removes
strings when their rectangle is cleared. Assertions incorrectly expected all
old terminal-state text to remain visible. The test now checks draw calls and
that the current state replaces earlier text, rather than weakening redraw
requirements. The FT8 row buffers add 2,672 bytes of external BSS; native ELF
placement and actual device behavior remain to be verified.

## 2026-10-26 native baseline and next controlled experiment

Native candidate `16ab1610` linked and passed the existing miniz/FT8/dependency,
40 KiB DMA and PSRAM placement inspections. App SHA-256:
`507a94fe1fe2af07fe1b38a566a55625ab96bdd306206891b39abfb3f00070c5`;
5,303,024 bytes, 988,432 bytes partition headroom. The two FT8 row buffers are
in external BSS, total 2,672 bytes. COM17 app-only flash, boot and non-app
preservation checks passed. COM39 has not been changed in this continuation.

The same bounded idle/picker/pan script on COM17 V3c, 906.875 MHz/960 ksps,
gain AUTO, RTL AGC/bias off, reproduced +16 driver overruns/drops and +554
application IQ pipeline drops. Idle before/after added zero. DSP block maximum
rose to 275,801 us during map work. These are **failed** continuity results.
Logs: `pr211-map-iq-baseline-16ab1610-com17-20261026.log`,
`release-review-com17-boot-native-monitor-20261026.log`,
`pr211-release-review-artifact-inspection-20261026.log` and
`release-review-{verify-previous,preservation}-20261026.log`.

CodeRabbit's full-review request was rejected: 352 files exceed its 100-file
limit (reply 6410351768). Removing filters restores coverage eligibility but
is **not** a completed external review. Arrange bounded test/shipping-doc review
passes and restore unrestricted configuration afterward; do not mark this gate
complete from the earlier source-only review.

Next experimental source bounds M5GFX's deliberately weak cache-writeback hook
to 4 KiB **per esp_cache_msync call**. IDF 5.5.4 releases its interrupt critical
section and outer cache mutex at each return. Its optional inner chunking leaves
the outer mutex held across the entire framebuffer flush, so the prior 128 KiB
SDK experiment was insufficient. No managed component, receiver rate, ring size,
SDK defaults or DMA reserve is changed. Host tests verify aligned complete
coverage, maximum chunk size, overflow and failure handling. Hardware benefit
remains unproven until the next comparison.

The source also separates download servicing onto one permanent worker with an
8 KiB PSRAM stack. A shared discovery/activation lock defers recovery/scanning,
not immutable archive reads or queued rendering. IDF FatFs uses per-volume
reentrant locking, and activation preserves the old archive. Host tests exercise
a blocked transfer alongside queued rendering, deferred discovery, and cleanup
at all six startup allocation/task failure points. Actual concurrent SD/render
and radio validation remain required.

A bounded review pass now includes the relevant map/SD/lifecycle tests, user
map/setup documentation, release notes and all source followups after 271dca27.
Its path manifest is saved outside Git at
`.local/maps/review-tests-docs-pass1-20261026.json`. Configuration must be restored
to unrestricted scope after this pass. Other historical evidence files are not
being represented as externally reviewed by this bounded pass.

The existing HTTP location-lookup regression initially failed to compile after
the new lifecycle APIs; its FreeRTOS stub now models task handles, suspension
and WithCaps deletion. Its sanitized CTest passes 1/1, including twenty repeated
actual worker completions with creation/deletion counts matched. Logs:
`pr211-location-lookup-{regression-red,build,test}-20261026.log`.

## Failed cache-flush hypothesis and stage-level investigation

Candidate `4ca5e23d` app SHA-256
`08992af768c2595fcd254d31de3c8b908d97f7cfb6e90f20c38f28c456b47b77`
passed native linking/strong-hook inspection, COM17 app-only preservation and
boot. It does **not** pass IQ continuity: the same V3c/960 ksps picker/pan test
added 38 driver drops and 688 application IQ queue drops. The writeback hook
reported no errors, max chunk 4,096 bytes, max chunk time 2,404 us and max call
5,941 us; DSP block maximum still reached 360,749 us. Therefore framebuffer
cache flush duration alone does not explain or resolve these losses. Do not
present this experiment as a release fix or retain it without a measured reason.
Logs: `pr211-map-iq-writeback-4ca5e23d-com17-20261026.log`,
`pr211-writeback-worker-artifact-inspection-20261026.log`,
`writeback-worker-com17-boot-native-monitor-20261026.log`.

All 16 configured map/UI host CTests pass after rebuilding their targets;
`pr211-map-host-{build,tests}-20261026.log`. This is the bounded map suite,
not whole-repository decoder qualification.

The next diagnostic adds peak time per existing DSP stage to `RTL_DSP STATS`
(28 bytes of counters), retaining averages, rates and loss accounting. Its
focused sanitized test reproduced the missing peaks and passes after adding
peak/reset coverage. No demodulation or decoder algorithm changes are made.
A temporary ENERGY-versus-PREAMBLE run is diagnostic isolation only and restores
the original PREAMBLE mode in a finally block; it cannot qualify LoRa reception.

## Review follow-up and isolation results

The 89-file CodeRabbit review completed as review 5648912293 on `7886eef2`.
Its ten findings plus the HTTP-stub nit and Grok's stack-safety finding are
classified in `docs/evidence/maps/2026-10-26-review-7886eef2-disposition.md`.
All still-valid source/test/doc findings have focused corrections prepared;
native/device validation of those corrections remains open. Working evidence
links were verified in the reviewed Git commit rather than changed blindly.

The download worker is now on-demand with an internal 8 KiB stack, reaped by the
UI after each job. This supersedes the earlier permanent-PSRAM-stack design.
Startup, blocked-transfer/render scheduling and allocation/reaping tests pass.
Four P4 syntax checks and all 16 configured map CTests pass after the corrections.

Additional IQ isolation does not close the gate:

- ENERGY detector mode reduced but did not eliminate map-associated losses:
  +6 driver drops and +75 pipeline drops. PREAMBLE was restored and verified in
  a finally-path capture. This is diagnosis, not decoder acceptance.
- Eight idle retained-map memory queries added zero drops; repeated heap status
  queries alone do not explain the stalls.
- A single authenticated session with PING keepalives still produced drops while
  panning, so repeated AUTH/journal writes are not required to reproduce it.
- Eight copies of the same completed map (generation unchanged, cached pushes
  1 to 9) added zero driver or pipeline drops. This narrows the investigation
  toward background detail generation/storage rather than the cached copy alone.

Logs beside invocation: `pr211-energy-isolation{-restore,}-com17-20261026.log`,
`pr211-heap-query-isolation-com17-20261026.log`,
`pr211-single-auth-map-isolation-com17-20261026.log`,
`pr211-cached-push-isolation-com17-20261026.log`.
The native `ba770de5` diagnostic reports long peaks across signal level,
spectrum-copy and decoder stages during detail generation; no change to decoder
algorithms or loss counters was made.

The bounded 89-file pass is complete. Unrestricted CodeRabbit path eligibility
is restored; the next incremental review must cover the subsequent source,
DSP diagnostics, test and documentation corrections without those exclusions.

## Current release decision - October 26, 2026

Owner: "I don't consider that a blocker. Small iq drops when moving the map are
fine." This supersedes the earlier zero-map-loss release criterion. Brief loss
associated with movement/detail refill is an accepted limitation, not a fixed
bug or a passing zero-loss measurement. Keep the failed experimental results as
historical evidence. Do not continue optimization or diagnostic build loops
solely to meet the superseded zero-loss criterion. Complete the outstanding
review, final-device validation and package gates before merge/tag decisions.

The unsuccessful application-level cache-writeback override is removed from the
shipping candidate after the owner's acceptance decision. The original M5GFX
hook and SDK defaults are restored; historical experiment results remain above.
The measured DSP stage peaks remain available for future diagnosis. No further
zero-loss optimization experiments are required for this release.


## Final candidate validation and follow-up

Candidate 00977434 booted on COM17 and COM39. App-only COM39 flash preserved NVS, PHY, coredump, world, bootloader and partition table by readback comparison. COM17's stationary LoRa map window (14,272 ms) kept cumulative IQ drops at 2,059 and driver overruns/drops at 74; these are unchanged counters, not zero lifetime loss. Log: pr211-shipping-stationary-map-com17-20261026.log. Three production catalog checks completed while rendering, with the internal download-worker stack reclaimed after each job.

CodeRabbit review 5656364274 found a valid existing-file FT8 log regression: the SD wrapper translated r+ into rb. The correction preserves update access and write buffering. The production-mode regression fails at the retry write before the fix and passes afterward; all nine FT8 log fault cases also pass. Commands: bash -lc 'python3 tests/storage_update_mode_test.py && python3 tools/test_ft8_log_storage.py'. A rebuilt artifact and device validation remain required after this correction. The review's replacement-character documentation finding is also corrected.


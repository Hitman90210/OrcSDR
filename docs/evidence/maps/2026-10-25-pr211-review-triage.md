# PR211 release review continuation

This is a partial triage, not a clean whole-PR review. Continue on PR #211.

| Finding | Classification | Current disposition |
|---|---|---|
| Settings removal confirmation survives hidden catalog changes (4372201757) | FIX BEFORE MERGE | Source corrected; focused regression and P4 syntax pass; not installed on device yet |
| Map/setup value updates clear the full screen (4372201746) | FIX BEFORE MERGE | Source corrected; recording-display regressions and P4 syntax pass; native/physical checks pending |
| Unauthenticated saved receiver coordinates in new map status (4372201751) | FIX BEFORE MERGE | Source corrected in picker and LoRa status; authenticated/unauthenticated regressions and P4 syntax pass; device check pending |
| FT8 append retry can duplicate partially persisted rows (review 5645729155, outside diff, main.cpp:13505) | FIX BEFORE MERGE | Fixed-position retry source corrected; fault-injection and P4 syntax pass; native/device checks pending |

The other earlier findings are classified below from bounded current-source
inspection. They are not fixed or waived merely because the LoRa review passed. The
latest CodeRabbit pass on a9e04c49 added no inline comments on the LoRa memory
changes, but did raise the outside-diff append finding above.

## Catalog confirmation correction

Observe catalog changes regardless of the currently visible Settings section,
and invalidate the armed removal against that observation. Repainting remains
limited to the visible Data & Maps section. Unchanged snapshots preserve an
armed confirmation, so the fix does not make confirmation impossible.

`python tests/settings_catalog_invalidation_test.py` reproduced the hidden-change
bug before the source correction, then PASS. It exercises the production change
condition/observation and the real removal cursor: a different pack occupying an
old slot requires fresh confirmation, unchanged state preserves confirmation,
and a hidden update does not repaint the other section.
`map_catalog_paging_tests` PASS (standalone WSL g++ command in local evidence).
P4 syntax for settings_app.cpp PASS. No installed map was removed in testing.
Combined native/device validation remains pending alongside other review fixes.

## Grok bounded second opinion

Grok CLI, using the installed grok-review workflow, returned CLEAN for the
three-source-file memory correction b0542203..a9e04c49 (190 added / 20 removed
lines). The packet included ownership facts and the proposed cache test; no
repository, shell, write or web actions were permitted. It was not a whole-PR
review and does not establish that cache chunking fixes IQ losses.

Raw reports outside Git, beside invocation:
`grok-lora-release-review-20261025.log`,
`grok-lora-release-review-20261025.md`,
`settings-catalog-hidden-{red,green,final}-20261025.log`,
`settings-catalog-p4-syntax-20261025.log`.
Review snapshots: `.local/maps/pr211-source-findings-current-20261025.json`,
`.local/maps/pr211-lora-{formal-review,review-findings}-20261025.json`.

Radio loss under maps remains open after the failed cache-option acceptance
trial. The approved decoder/dongle release-runner extension, full fixture/source
audit, qualified RF matrix and tagged-package gates must still be completed.
No new PR, main merge, release tag or package publication occurred here.

## Map status and redraw corrections

Read-only map status retains active/readiness/zoom information for unpaired
peers. Exact saved/proposed coordinates and the LoRa followed-node ID require
the existing authenticated session. Both previous privacy regressions failed
first, then passed with the scoped output guards. This is not an app-wide
serial privacy audit.

Map-screen redraw now distinguishes entry/mode/step/editor transitions from
ordinary value changes. Ordinary setup, inventory and download changes replace
the content region rather than clearing the full screen; a valid picker map
remains visible during ordinary updates. Empty download taps and scrolls already
at their boundary do not repaint. No new framebuffer or heap buffer was added;
layout tracking is one bool and one uint32_t. Physical flicker acceptance is
still required. The recording stub now removes strings covered by rectangular
clears, so it checks disappearance of old text as well as new visible values.

Executed commands (worktree root, raw logs beside invocation):

- `wsl cmake --build /mnt/f/AI/OrcSDR-TEMP/maps-integration-completion/.local/maps/feature-replay-integration-20261023 --target orcsdr_maps_tests orcsdr_lora_map_tests orcsdr_setup_ui_tests -j 4`: PASS.
- `wsl ctest --test-dir /mnt/f/AI/OrcSDR-TEMP/maps-integration-completion/.local/maps/feature-replay-integration-20261023 -R map_pipeline --output-on-failure`: PASS, 1/1. Recording regressions cover repeated download messages, unchanged/animation ticks, inventory revision, setup network polling, no-op touches and entry/return behavior.
- Same CTest command with `-R lora_map_controls`: PASS, 1/1; unauthenticated status hides coordinates/node ID, authenticated output remains available.
- Same CTest command with `-R setup_ui`: PASS, 1/1.
- `python .local/maps/check_map_review_syntax_20261025.py map_screen.cpp map_serial.cpp lora_dashboard.cpp`: PASS for all three P4 compilation units.
- `git diff --check`: PASS.

Logs: `map-privacy-red-test2-20261025.log`, `lora-map-privacy-red-20261025.log`,
`map-redraw-{red,green3}-20261025.log`, `map-review-focused-build-20261025.log`,
`map-review-p4-syntax-20261025.log`. Device still runs the previously verified
5e79fd44 app; these source corrections await a combined native build/flash.

## FT8 CSV retry correction

The staged batch records its original file offset before its first write.
Retries reopen the same daily file read/write and seek to that offset instead
of appending the whole batch again. A partial new-file header is rewritten too.
The offset resets only after successful write, sync and close. Missing pending
files, status errors, or a file shorter than the saved offset fail closed.
No decoder changes, audio fixtures or new logging format are involved.

`python tests/ft8_log_retry_test.py` first reproduced corruption after a partial
header, then PASS against the production flush function. Cases cover partial
header/data writes, sync and close failures, additional staged rows before retry,
open/seek failures, unavailable file status and a missing pending file. The test
also verifies exact complete CSV bytes, not only return codes. Address/undefined
sanitizer run PASS. Main-file P4 syntax PASS.

Scope: in-session retries with the application as sole writer. RAM staging and
the retry offset are not journaled across power loss, so crash recovery and
concurrent external writers are not claimed. SD hardware validation remains
separate. Logs beside invocation: `ft8-log-retry-{red,final,sanitized}-20261025.log`
and `ft8-log-retry-p4-syntax-20261025.log`.

## Remaining earlier findings: bounded source verification

All are FIX BEFORE MERGE; none has been silently downgraded to OPTIONAL/STALE.

| ID | Current evidence | Next scoped check |
|---|---|---|
| 4372201699 | Temporary source-only `.coderabbit.yaml` filters still present | Review tests/shipping docs, then restore broad filters |
| 4372201703 | ADS-B zoom still calls full `redraw()` and reports settings_changed at range limits | Recording test for map-only zoom and no-op bounds |
| 4372201719 | FT8 globe still calls readPixel/drawPixel for every output pixel | Row-transfer equivalence/byte-order test before optimization |
| 4372201722 | FT8 pending publication leaves revision unchanged; update retries the map tab | Recording test for pending retries without static-panel clears |
| 4372201726 | Location worker uses WithCaps task allocation but ends with vTaskDelete | Task ownership/reaping fault tests against installed IDF 5.5.4 before changing deletion |
| 4372201727 | LoRa label toggle now updates only its button and map region | Recording regression/P4 syntax/native linking pass; device check pending |
| 4372201733 | queue_local_rtl_listen returns immediately under setup_receiver_inhibit; later I/O deferral excludes setup_radio_paused | Lifecycle test for external navigation while setup is paused |
| 4372201738 | OrcDial TX task failure leaves null handle without setting initialization_retry_ms | Allocation-failure/backoff test; preserve paired-device behavior |
| 4372201742 | Download notice x318..412 intersects FT8 title/divider and LoRa divider | Header-geometry/recording checks for notice appearance and removal |
| 4372201745 | FATFS capacity query and direct free-space callers now corrected in source | Host failure/full-card tests, transfer/shared-SD checks and native linking pass; device validation pending |
| 4372201755 | downloads::service executes synchronously ahead of map queue receive | Bounded concurrent transfer/render scheduling with shared-storage ownership tests |

The source inspection does not endorse the review's suggested implementation.
In particular, task self-deletion and transfer/render concurrency need their
own narrow ownership tests; do not replace them with an unverified one-line fix.

## Additional host checks requested while building

All invoked from the integration worktree; raw logs remain beside invocation.

- Built and ran `map_preferences`, `location_store`, `sd_dma_buffer` and
  `setup_flow` individually in the existing WSL host build: PASS, 4/4.
  Logs `release-extra-map-{build,tests}-20261025.log`.
- Built catalog-security/fetch/NVS targets using the configured Visual Studio
  generator with `C:/Program Files/CMake/bin/cmake.exe`. CTest selector
  `map_catalog_security|map_catalog_fetch|map_delivery_nvs` matched and ran
  **two** registered checks: fetch and delivery/NVS, PASS 2/2. The signature
  executable built but was not run by that selector; no signature-test pass is
  claimed. The first attempt used the IDF CMake, which lacked the configured
  VS18 generator. Logs `release-extra-catalog-{build,tests}-retry-20261025.log`.
- `wsl python3 /mnt/f/AI/OrcSDR-TEMP/maps-integration-completion/tools/test_ft8_log_storage.py`:
  PASS, nine sanitizer-enabled fault cases. The existing fake filesystem needed
  status/seek parity, and each independent scenario now runs in a fresh process
  so static retry state is not accidentally shared between synthetic files.
  Sequential retry/data-integrity behavior remains covered by the new byte test.
  Logs `release-extra-ft8-storage-{initial,final,retry}-20261025.log` preserve
  the stale-fixture failures and corrected result.
- `wsl bash /mnt/f/AI/OrcSDR-TEMP/maps-integration-completion/tools/test-sd-storage.sh`:
  PASS, five focused storage programs with address/undefined sanitizers: DMA
  buffer, write plan, LoRa history, SSTV archive and FT8 log. No RF, device or
  decoder recordings. Summary `sd-storage-20261026T011203Z.log` (UTC-stamped
  filename; local test date October 25) and `release-extra-sd-console-20261025.log`.
- Combined native source 4b2986e6: build and inspection PASS. App 5,299,952 bytes,
  headroom 991,504; SHA-256
  `1e9cf0a2c648d84c38de608e4bd4077c54e31e314c6ec778848b1bc24e24e41a`.
  Original shipping cache configuration, 40 KiB DMA reserve, production catalog,
  dependency pin and symbol/PSRAM placement gates retained. Artifact
  `.local/maps/release-review-4b2986e6-20261025/manifest.json`;
  logs `release-review-native-{build,inspection}-20261025.log`.

These results do not close native/physical UI acceptance, live storage fault
recovery, map-associated RF drops, decoder/dongle qualification or package gates.

## Combined COM17 candidate check

Source 4b2986e6, app SHA-256
`1e9cf0a2c648d84c38de608e4bd4077c54e31e314c6ec778848b1bc24e24e41a`
was flashed app-only on COM17. Prior app verification, fresh non-app backups,
post-flash app/non-app verification and normal startup PASS. SD, NVS, world and
C6 preserved; COM39 untouched. The installed app includes 513f68f5's earlier
catalog-confirmation correction as well as f02d4bc3 and 4b2986e6.

The bounded named-command check PASS: cold unauthenticated status hides exact
coordinates and followed-node identity; authenticated status returns the saved
point. Setup Welcome/Network/Location, Cancel, LoRa map pan, picker pan/Cancel
and Traffic return completed. The saved location stayed unchanged throughout.
The recording-display tests prove full-clear behavior on host; physical flicker
acceptance is recorded below separately. No hardware partial-write fault was
injected, so the FT8 storage failure-path claims remain host fault-injection.

After map activity, driver counters showed four drops/overruns at 960 ksps.
Thus this candidate does not resolve the separate radio-continuity gate. LoRa
decoder failure count was zero but no ambient packets arrived in the short
initial check; that window is neither positive decoder qualification nor a
failure. PSRAM free 3,168,104 / largest 1,441,792 with map state retained.

Logs beside invocation: `release-review-{preflash,device}-20261025.log`,
`release-review-flash-console-20261025.log`,
`release-review-boot-native-monitor-20261025.log`,
`release-review-device-result-20261025.json`.
Artifact `.local/maps/release-review-4b2986e6-20261025/manifest.json` retains
the exact source/app and recovery hashes. Bench handoff restores prior manual
gain 49.6 dB and LoRa Traffic without modifying receiver location or pairing.

## Physical acceptance: setup/map redraws

On October 25 the user checked COM17's updated setup Wi-Fi/status and Get Maps
pages and confirmed: **"Looks steady and responsive."** This closes physical
flicker/readability/responsiveness acceptance for those tested pages on the
installed 4b2986e6 app. It does not close unrelated dashboard redraw findings,
map-associated IQ losses, SD fault recovery, or the decoder/dongle release matrix.

The subsequent bench handoff also observed two CRC-valid LoRa packets with zero
decoder failures, including NODEINFO; see `release-review-handoff-device-20261025.log`.
This is bounded ambient reception evidence, not controlled packet qualification.

## SD free-space correction (source only)

`sd_space` uses the installed SDK's `esp_vfs_fat_info("/sd",...)` to query FATFS
total/free bytes. Wrappers return zero for unmounted/query failures and invalid
free>total results. `used_bytes` now subtracts values from a single successful
snapshot. Download preflight, SSTV, catalog initialization and Settings free-space
display use direct `free_bytes()` instead of separate total/used queries. No
managed component or SD format/mount configuration changed. The SDK query can
read FAT metadata; it does not format, remove or write user files.

Tests executed:

- `python tests/sd_space_query_test.py`: PASS. Nearly full/full/empty cards,
  32 GB arithmetic, invalid query values, I/O error, and unmounted state.
  Address/undefined sanitizer run PASS. The test was added first; initial red
  log records the missing query wrapper before implementation.
- `python .local/maps/check_sd_space_syntax_20261025.py orcsdr_storage.cpp map_downloads.cpp sstv_archive.cpp main.cpp`:
  PASS for all four real-P4 units.
- `cmake --build .local/maps/catalog-security-20261019 --config Debug --target map_transfer_test -j 4`
  using `C:/Program Files/CMake/bin/cmake.exe`, then matching CTest `-C Debug -R ^map_transfer$ --output-on-failure`:
  PASS, 1/1. Includes insufficient-space rejection before HTTP open and existing
  resume/activation cases. Uses only its guarded owned host fixture directory.
- `wsl bash /mnt/f/AI/OrcSDR-TEMP/maps-integration-completion/tools/test-sd-storage.sh`:
  PASS, now 6 programs: DMA, write plan, LoRa history, SSTV archive, FT8 log, and
  the newly registered capacity query. Existing programs retain sanitizers.

Logs beside invocation: `sd-space-query-{red,green,sanitized}-20261025.log`,
`sd-space-p4-syntax-20261025.log`, `sd-space-transfer-{build,tests}-20261025.log`,
`sd-space-shared-regressions-20261025.log`,
`sd-storage-20261026T020646Z.log` (UTC filename; October 25 local).

No full real SD card, map download, native rebuild or flash was used to establish
these results. This is a preflight check, not a storage reservation: concurrent
writers may still consume space later. Write/sync/close failure handling and
resume validation remain mandatory. COM17 retains app source 4b2986e6; COM39 is
unchanged. Installed-device confirmation of this correction remains pending.

## LoRa label-toggle redraw (source only)

The Labels button now redraws just that control and the map/overlays. Header,
tabs and selected-node panel are retained. No framebuffer was added. The
recording regression first reproduced the full-screen clear, then passed four
alternating toggles plus a failed save and successful retry. It checks the
visible ON/OFF/SAVE FAILED text, the labels flag passed to the map adapter, and
an unchanged full-screen-clear count. Existing drag, projection, selection,
ownership and serial-status tests remain passing in the same executable.

Commands: build `orcsdr_lora_map_tests` in the existing WSL maps build, then
CTest `-R lora_map_controls --output-on-failure`: PASS 1/1.
`python .local/maps/check_lora_label_syntax_20261025.py lora_dashboard.cpp`:
P4 syntax PASS. Logs beside invocation:
`lora-label-{red,green}-20261025.log`,
`lora-label-{red,green}-build-20261025.log`, `lora-label-p4-syntax-20261025.log`.

Existing named `RTL_MAP LABELS LORA ON|OFF` remains available; no new input or
authentication mechanism was added. Physical label-toggle acceptance and native
linking remain pending with the SD-capacity source correction. COM17 still runs
4b2986e6. ADS-B zoom remains separately open: it also discards its cached panel
before a pending replacement, so merely swapping full redraw for content redraw
would not fully address that finding.

## Setup direct-start pause guard

CodeRabbit review 5646376764 on 607410c6 added no inline comments, but its
outside-diff major finding identified direct `RTL_LISTEN`/`RTL_CAPTURE` requests
that bypass the navigation-level setup guard. Current native and legacy driver
loops consumed `rtl_capture_requested` without first checking `radio_io_paused`.

Both start conditions now test the existing shared pause predicate before
exchanging the request. Requests remain pending during setup, Wi-Fi or other
exclusive I/O pauses; they are consumed once the pause ends. No driver API,
task priority, capture format or receiver configuration was changed.

`python tests/receiver_start_pause_test.py` extracts both actual production
conditions. It failed when a paused request was consumed, then PASS after the
two guards: repeated paused polls retain the request, resuming consumes once,
and absent receivers also retain requests. Existing
`wsl python3 /mnt/f/AI/OrcSDR-TEMP/maps-integration-completion/apps/orcsdr-tab5/tests/setup_radio_pause_tests.py`
PASS: idle behavior, Wi-Fi overlap in both exit orders, active restore and stop
failure. Main-file real-P4 syntax PASS. Logs beside invocation:
`receiver-start-pause-{red2,green,lifecycle}-20261025.log`,
`receiver-start-pause-p4-syntax-20261025.log`.

This is separate from earlier navigation finding 4372201733, which remains open:
external dashboard selection during setup must also reconcile the requested
band and touch/UI ownership. The added start gate alone does not close that.

## Held SD/label native artifact

Source 8cd9306f native build and inspection PASS: app 5,300,528 bytes. Existing
40 KiB DMA reserve, original cache configuration, dependency/public-key/catalog,
PSRAM placement and symbol gates retained. Artifact
`.local/maps/release-sd-label-8cd9306f-20261025/manifest.json` and logs
`release-sd-label-native-{build,inspection}-20261025.log`.

It was deliberately not flashed after the direct-start finding was verified.
The pause guard must join the next combined build/device test. COM17 continues
running verified 4b2986e6, and COM39 was not accessed during this work. Do not
describe the source fixes as installed or the release gate as complete.

## Subsequent FT8 map memory validation

The installed state above is superseded: COM17 now runs `6f625f66`, including
the prior SD-space, label, pause-guard and FT8-map fixes. Installation does not
close each fix's distinct hardware gate. The new FT8 memory handoff did pass
three complete LoRa↔FT8 Map round trips: full LoRa buffers allocated on entry,
released on every FT8 return, and world geography rendered without a reboot or
manual reclaim. The first candidate exposed an outgoing-FT8 cleanup-order bug;
the final candidate corrects it. See
[the scoped evidence](2026-10-25-ft8-map-memory-recovery.md) for hashes, commands,
logs and limits. Overall PR review, radio/drop and release-matrix gates remain
open; no merge, tag or full release approval follows from this result.

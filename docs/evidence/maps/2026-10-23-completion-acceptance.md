# OrcMaps completion acceptance — October 23, 2026

**Latest October 24 checkpoint:** COM17 now runs `46cf8148` configured for the
[published production catalog with no expiry](2026-10-24-production-catalog-activation.md).
Build, preservation, production Check and repeat Check after reboot pass. One
global pack is available; existing pilot pack remains usable but the changed
production manifest is not installed. No repeat archive transfer. Standard build
helper explicitly sets production URL/key; firmware/M5Burner release packaging
and off-machine key backup remain open. Earlier production-unconfigured claims
below are historical. Current [20-pan result](2026-10-24-offscreen-native-refill.md)
still fails p95 ≤2s (3.148265s); physical dragging/colors/controls were accepted
for that engine. Other unclosed gates remain open.

Latest source reconciliation: local merge `97f48640` includes fetched main
`fb70e969` (nine clock/WWV follow-up commits). Seven selected host checks and
six P4 syntax checks pass; no build or hardware operation in this step.
See [reconciliation evidence](2026-10-23-mainline-clock-reconciliation.md).
COM17 now runs source `137e244a`: [native build/preservation/startup evidence](2026-10-23-reconciled-native-candidate.md). Build, inspection and app-only preservation/startup pass; latest loaded timing still fails the two-second gate and physical acceptance remains open. Earlier candidate entries below are historical.

Latest loaded result: [October 24 reconciled 20-pan measurement](2026-10-24-reconciled-refill.md): p50 2.866 s, p95 2.891 s, max 2.895 s. The two-second gate still fails; driver drops/overruns were zero in captured snapshots. That pan run returned COM17 to stopped setup. The later clean GitHub installation test is complete; see the separate baseline below.

[October 24 native setup navigation](2026-10-24-native-setup-navigation.md) passes Next/Back/Skip/Quick/Cancel and out-of-order Finish rejection, with unchanged saved coordinates. Entry/lookup, successful Finish/recovery, no-SD and physical hotplug remain open.

[Address/IP provider decision](../../maps-delivery/2026-10-24-location-provider-decision.md) records the hardcoded endpoints, public-service scaling/switching gap and recommended owned endpoint. Native overlength-query rejection passes; no real address submitted or provider configuration changed. Production lookup remains open.

[October 24 clean GitHub installation baseline](download-benchmarks/2026-10-24-com17-pny32/README.md) passed on the spare PNY 32 GB U1 card: transfer 19m40.3s, verification/activation 21m21.2s, total 41m02.6s. Controlled resume, reboot/offline regional rendering and physical appearance passed. Reception began partway through verification; this is a mixed-load observation. Three driver drops/overruns in the separate post-reboot follow-up remain recorded; timed-install final counters were zero.

[Prepared verification progress feedback](2026-10-24-verification-progress-feedback.md) adds real checked-byte/pass display and serial fields. Focused UI/integrity tests and six P4 syntax checks pass; no native build/flash or physical acceptance for this source yet. Baseline firmware/timing records remain unchanged.

Production delivery proposal prepared: [hosting, signing and 20,000-user model](../../maps-delivery/2026-10-23-production-decision.md).
GitHub Releases is approved as the initial hosting choice; production key creation and publication remain unapproved.
Arithmetic and local links pass `maps-production-decision-check-20261023.log`.
No account, domain, key, publication or device change was made for the proposal.

Initial scout source 4d4f85a3 on codex/maps-integration-completion.
Reconciled source 30a00ce1 now contains fetched main 629ba546; affected host/
sanitizer, P4 syntax, native link, app-only COM17 preservation/startup and bounded
map navigation checks pass. Earlier COM17 candidate was f38b8601 (replay engine
2133547); the prior 30a00ce1 dragging/colors/controls were physically accepted.
This exact candidate's appearance remains unverified. Its 20-pan p95 is 3.55 s,
so the two-second gate still fails. Retained SD/reclaim/fallback checks pass.
Decoder testing was excluded by the user.
Current OrcMaps pin is 2133547bf8e003e2f4c52de3b8abf737261a8397. This matrix supersedes
stale unchecked entries in the historical plan without deleting their evidence.

| Area | Implementation | Recorded validation | Physical acceptance / remaining gate |
|---|---|---|---|
| Location picker/save | Implemented; verified explicit lat/lon persistence and feedback | Bounded COM17 save/return/reboot checks | Prior controls, readable text, saving feedback and dragging accepted; retain on final candidate |
| Shared map consumers and labels | LoRa, ADS-B and FT8 use shared service; independent preferences | f38b8601 native label independence, reboot persistence, exact restoration and location retention PASS; prior host/COM17 map checks exist | Label preference persistence closed; physical label appearance across all consumers, ADS-B aircraft/range and FT8 projection/whole-world/polar acceptance remain open |
| LoRa map overlays | Bounded priority names, markers, centering and Follow implemented | Controlled text/NODEINFO and bounded position/Follow checks | Prior dragging/flicker accepted; actual dashboard host regression passes identity retention across reordered/missing/returning nodes; native reordered-traffic proof remains open |
| Detail refill/read cache | 256 KiB read cache plus optional 768 KiB reclaimable feature replay | 137e244a 20 pans: p50 2.866 s / p95 2.891 s / max 2.895 s; all six replay hits, zero tile reads. Prior f38 retained SD/reclaim/fallback pass | Two-second gate FAILS; cached motion remains separate. Replay enabled during latest run; prior reclaim guard/fallback evidence retained. Exact candidate physical acceptance open. No isolated speed multiplier claimed |
| Cached motion | Last-valid-frame preview and clipping implemented | Pan paint checks exist; representative continuous timing incomplete | Approximately 33 ms target remains open; paint-call timings are not full motion latency |
| Worldwide quality | Profile, hierarchy, rings/clipping and bounded labels implemented | Fixed worldwide host views recorded | Device boundaries/contrast/international-script and fallback acceptance still open |
| Setup | Wizard-owned methods and reception pause implemented | Bounded coordinate confirmation, IP proposal, exit/reception and graphics checks | Full first boot/rerun, credentials/address, Back/Skip/Cancel/recovery, audio and paused hotplug open |
| Delivery | Signed TEST-ONLY pilot and real installer connected | 543380195-byte HTTPS install/resume/activate/reboot/offline checks pass | Production key/hosting decisions unresolved; full/removed SD, corruption/interrupted activation/version replacement hardware checks open |
| Install latency | Safe verification implemented | Prior approximately 12-minute observation; new mixed-load PNY baseline 21m21.2s post-transfer | Reduce or explicitly resolve without weakening verification |
| Serial parity | Named shared map/setup controls implemented | Focused source/host/device checks; latest restore uses RTL_LISTEN LORA | Reconcile stale coverage ledger and close unverified action/UI equivalence |
| SD/memory/radio | Aligned read cache, no new internal DMA pool; reclaim/reset paths implemented | Native 16 KiB retained-memory paused write/readback; active map reads; radio/log recovery pass | Final shared-candidate budgets, pressure/navigation checks open; historical failures preserved |
| Mainline reconciliation | Isolated merge 30a00ce1 reviewed, then local integration fast-forwarded | Five conflicts resolved; maps 5/5, FT8 normal/sanitized, two clock tests and P4 syntax pass | Native build/inspection, app-only preservation/startup and bounded COM17 navigation PASS; main and remote refs unchanged; broad final review remains open |

## October 23 scout

Tracked tree clean before this document; earlier untracked logs, binaries,
recovery images and the draft goal are preserved. No active native build/monitor
process was found among the named processes checked. The app remains a last-tested
state, not a claim of today's live hardware state.

Fetched origin/main is 629ba546. Integration is 48 commits behind main and 58
commits ahead of origin/codex/maps-integration-completion. Eight paths overlap:
main CMakeLists, ft8_dashboard.cpp, ft8_runtime.cpp, main.cpp,
screen_controller.cpp/.hpp, tests/ft8_dashboard_tests.cpp and tools/test-ft8.sh.

Exact simulation: git merge-tree --write-tree --name-only HEAD origin/main.
Expected conflict exit 1; log maps-mainline-reconciliation-scout-20261023.log in
the integration root. It creates an unreferenced tree without changing the index,
working files or branches. Five conflicts: FT8 chunk placement (equivalent),
Home paint ownership (retain Maps and Dead Air), screen ID/name registration
(retain both), independent map/clock test additions. Auto-merges still require
scoped checks, especially FT8 clock versus map-label header layout/hit routing.

Risk: medium for scoped isolated reconciliation; high for a whole-branch review.
Prepare a separate candidate under F:/AI/OrcSDR-TEMP. Preserve shared branch/main
refs. Run only affected FT8/maps/navigation checks first. Full branch review,
production decisions, erase and main merge retain their separate approval gates.

## Next milestone

Reconciled candidate has passed its bounded native gate; source acceptance does
not close the remaining rows above. The numeric gates are defined below; reduce
repeated decode/draw work within measured memory limits on authorized COM17. Do not add an arbitrary overnight soak or
restore legacy geography. Production/publication choices remain decision-blocked,
not reasons to stop independent authorized implementation.

Scoped reconciliation results: [source and tests](2026-10-23-mainline-reconciliation.md).

## October 23 bounded native checkpoint

COM17 30:ED:A0:E2:E6:95, application 30a00ce1: named Local Maps ownership,
LoRa map pan/return, ADS-B map/range and FT8 map/label-off/on/live return pass.
Completed frames all report missing=0 and failed=0. Observed detail-frame wall
times: LoRa 3.368263 s, ADS-B 3.268251 s, FT8 0.478210 s. These are single
observations, not p50/p95 acceptance or full motion latency. FT8 STATUS reports
runtime=stopped, so no FT8 decoder-load/stack-fault reproduction is claimed.

Saved point remains E7 440628866,-1230160273; label preferences remain
picker=1,lora=0,adsb=1,ft8=1. ADS-B range remains 25 nautical miles. FM restored
at 94.5 MHz, manual gain 445, driver STREAMING, drops=0/overruns=0. Post-navigation sampled
PSRAM free 2,144,780 bytes/largest 1,245,184; largest DMA 19,456; main stack
high-water mark 1,448; monotonic uptime and unchanged reset reason 11. Retained
allocation pressure requires a separate repeat/reclaim check. LoRa SD logger
ready=true,error=false,queued=0,dropped=0. Antenna suitability was not confirmed
for this run: these results demonstrate stream/UI continuity, not RF decoding.

Exact postflight command (integration root):
`python .local/maps/com17_label_bench.py maps-mainline-postflight-plan-20261023.json maps-mainline-postflight-device-20261023.log`
Exit 0; console companion maps-mainline-postflight-console-20261023.log.
See the linked reconciliation record for build, hashes and preservation logs.

Follow-up named memory query on Home identifies the retained holder: LoRa IQ
buffers 15,840,000 bytes, map frame 1,048,576 bytes. FT8 reports active=0,
last_failure=memory, needed=516000, failures=12. Thus FT8 decoder startup after
this navigation sequence FAILS despite its map/UI checks passing. This is a real
acceptance blocker; runtime=stopped must not be counted as receiver continuity.
The existing explicit FREE MEMORY / RTL_MEM RECLAIM path is being checked before
any source change. Its LoRa guard preserves active/captured/exporting data.
Return-Home query log: maps-mainline-return-home-device-20261023.log, command
`python .local/maps/com17_label_bench.py maps-mainline-return-home-plan-20261023.json maps-mainline-return-home-device-20261023.log` (exit 0).

## Accepted responsiveness gate

User selected October 23: p95 detailed refill within 2.0 seconds for a fixed
COM17 routine-pan workload under reception. Cached visible-motion updates retain
a separate approximately 33 ms target. Report p50/p95/max with sample count and
nearest-rank percentile method; do not substitute LCD push timing for end-to-end
motion. Preserve detail/pixel output, cancellation, memory safety and RF continuity.
No new optimization is accepted solely on desktop timing.

Existing explicit reclaim check exits 0. RTL_MEM RECLAIM releases 16,888,576
bytes (LoRa 15,840,000 + map 1,048,576); FT8 then starts, reports listening,
tap=1 and 123 incoming blocks after 10 seconds. Sample free PSRAM recovers to
16,234,296 bytes; largest 15,204,352, main-stack high-water 1,448. No decoder
success is claimed from this short run. This confirms safe manual recovery,
not automatic pressure handling: switching directly from retained LoRa/map
allocations can still require FREE MEMORY. Preserve this as an explicit remaining
cross-dashboard acceptance issue rather than silently refactoring receiver lifecycle.

Command: `python .local/maps/com17_label_bench.py maps-mainline-reclaim-plan-20261023.json maps-mainline-reclaim-device-20261023.log`
Exit 0. Plan ends by restoring 94.5 MHz FM/manual gain 445 and Home.

## Scope clarification — October 23

User confirmed newer FT8 decoder work is already merged into main and explicitly
removed decoder testing from this goal's execution. Refetched origin/main remains
629ba546, already included in reconciled 30a00ce1. The observed allocation/reclaim
result above is retained as honest navigation/memory evidence, not a request to
reopen decoder validation or refactor its lifecycle. Further tests target maps,
UI ownership, retained memory and radio continuity only.

Measured follow-up: [20-pan refill baseline](2026-10-23-refill-baseline.md) records p50 3.65 s / p95 4.55 s / max 5.13 s; the 2-second gate fails.

User physical acceptance of reconciled application30a00ce1 during the pan run: “Dragging, colors and controls look good.” This does not accept the separate, uninstalled single-inflate experiment.


Latest experiment: [single-inflate rejected and accepted app restored](2026-10-23-single-inflate-rejected.md).

Further feasibility evidence: [pan overlap and decoded-geometry footprint](2026-10-23-pan-reuse-feasibility.md).
Eight of twelve label-off shifted overlaps differ from a complete redraw. The
six-tile reference view has 1,509,416 bytes of point coordinates alone; neither
naive overlap reuse nor an unbounded whole-geometry cache is accepted. No device
operation or additional firmware change was made for these probes.

The same evidence now includes a compact replay prototype: ten full-frame
label-off comparisons have zero differing pixels at the reference and
antimeridian views. The reference payload is 433,391 bytes, but vector capacity
is 573,440 bytes; Tokyo exceeds a 512 KiB payload budget. Fixed-capacity storage,
fallback/eviction, labels, cancellation/reclaim and native timing remain open.
This is a host feasibility result, not an installed optimization.

Next source checkpoint: [bounded shared-pipeline integration](2026-10-23-feature-replay-integration.md).
Focused integration passes 2/2, including replay/streaming equivalence, fallback,
eviction and cancellation recovery; three P4 syntax checks pass. Engine dependency
resolution, native link/placement and COM17 performance remain open. A full
producer host run and development push are now explicitly approved.

Latest native gate: [f38b8601 build, COM17, 20-pan failure and retained SD/reclaim](2026-10-23-feature-replay-integration.md).
OrcMaps full suite passes 9/9 and approved producer revision 2133547 is published
on the development branch. Consumer pin/lock resolve through the component
manager. No main merge or release. COM17 is receiving with normal streaming
fallback after reclaim; the complete goal and other acceptance rows remain open.

Later bounded native check: [independent label persistence](2026-10-23-independent-label-persistence.md)
passes four independent setting changes, a controlled reboot and exact original
preference restoration, with saved receiver location retained. No flash or decoder
test. The initial missing-COM17 guard rejection is preserved; the user reconnected
the reference device before the passing run. COM17 remains f38b8601; producer
6a621a7 full-regression/publication approval and other acceptance gates remain open.

Latest mainline scout: origin/main now points to fb70e969, nine FT8 clock/WWV
follow-up commits beyond the incorporated 629ba546. The scoped map-download
buffer change has no upstream file overlap. A merge-tree simulation exits 0
without conflicts (maps-mainline-followups-scout-20261023.log); no branch/ref or
working source was merged/rebased. Integration review/targeted clock checks and
native validation of a reconciled candidate remain open. The WWV audio fixture
was not inspected and FT8 decoder testing remains excluded.
Current app remains 30a00ce1 / engine b59d478; consumer source revert ff31e14b restores
that app tree exactly. Engine PR7's optional path was withdrawn after an 8.46 s
completed frame and incomplete seven-second samples. No candidate percentiles
are claimed. Restoration, four bounded pans, saved point/preferences, map reclaim
and RF/logger checks pass. CodeRabbit's full review completed with 4 actionable
comments; bounded triage awaits the separately requested AGENTS approval.

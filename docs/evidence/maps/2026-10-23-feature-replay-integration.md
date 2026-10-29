# Shared pipeline feature replay — October 23, 2026

Prepared consumer uses OrcMaps 2133547 (replay core plus overlapping arena
compaction). The tracked consumer pin/lock have not yet been updated at this
source checkpoint; this is not a linked firmware or a merge-ready dependency
state. Current COM17 remains accepted app 30a00ce1. No device access occurred.

## Implementation and limits

FeatureCache keeps at most 64 tile entries in caller-owned metadata. Before
rendering it evicts entries outside the visible tile set or with a different
place-layer mode, then compacts surviving records in memory order. Only a
successful stream and completed ordered drawing publish a tile. Failed append,
decode, render or cancellation cannot publish an incomplete prefix. Exhaustion
streams normally. Reopen/profile change invalidates entries; native source reset
also releases the arena. Reclaim detaches all source/frame allocations.

The native adapter supplies a single optional 768 KiB PSRAM arena. Metadata is
external BSS, runtime-constructed; Pipeline has only an extra pointer rather than
putting the entry array on a probe's stack or the internal heap. Allocation
requires room for the arena, 512 KiB remaining headroom and any missing preview.
This guard is a proposed budget, not hardware acceptance. The existing 40 KiB
DMA reserve, RTL driver, radio servicing, retained preview and SD read cache are
unchanged. Canvas resize releases replay before allocating mandatory storage.

## Tests actually run

Invocation worktree: F:/AI/OrcSDR-TEMP/maps-integration-completion.

```text
wsl -d Ubuntu -- bash -c 'cd /mnt/f/AI/OrcSDR-TEMP/maps-integration-completion && cmake --build .local/maps/feature-replay-integration-20261023 --target orcsdr_maps_tests orcsdr_map_feature_cache_tests -j2 && ORCSDR_LABEL_TEST_ARCHIVE=/mnt/f/AI/OrcSDR-TEMP/orcmaps-quality-delivery/.local/global-assessment/global-z8-candidate.pmtiles ctest --test-dir .local/maps/feature-replay-integration-20261023 -R "^(map_pipeline|map_feature_cache)$" --output-on-failure' > maps-feature-replay-integration-cancel-20261023.log 2>&1
python .local/maps/check_feature_replay_syntax_20261023.py
```

Both exit 0. CTest passes 2/2. Its preserved detailed output is
.local/maps/feature-replay-integration-20261023/Testing/Temporary/LastTest.log.
Ten reference-view pan/label comparisons match streaming pixels and measured
label-name sequences. Warm pans hit all six tiles, retaining 702,110 bytes.
Additional boundary-crossing pans, one-byte-arena streaming fallback, detach/
reclaim fallback, reopen invalidation and cancelled-replay recovery pass.
The text fixture records layout names; actual FreeType glyph equality was tested
separately against the engine core, not claimed by this recording target.

The focused cache test also passes ASan/UBSan/leak checking: command and output
maps-feature-cache-sanitized-20261023.log. It tests overlapping compaction,
failed append, cancelled publication, surviving-label integrity and mode eviction.

P4 syntax log: maps-feature-replay-p4-syntax-20261023.log. Pipeline, map_service
and feature_replay.cpp pass using explicitly selected local engine headers and
the existing compiler database. This is not a resolved native link or a memory
section check. The full nine-program OrcMaps run/publication is separately
approved and running at this documentation checkpoint.

## Observability and remaining gate

RTL_MAP STATUS now includes ORC_MAP_REPLAY with capacity_bytes, retained_bytes,
tile_hits and fallbacks for the last frame. Held/reclaimed memory includes the
arena. No new user action or authentication path is introduced.

Resolve approved producer publication, regenerate the pin/lock, build and inspect
P4 section placement/size, then verify COM17 identity/recovery and perform the
already-authorized app-only update. Measure the unchanged 20-pan workload under
reception, cancellation, low-memory fallback/reclaim, SD and radio continuity.
Do not infer two-second acceptance or a speed multiplier from these host results.

## Producer gate and dependency resolution

User approved the full producer suite and publication of the two commits through
2133547. The configured OrcMaps host build and CTest pass 9/9, exit 0; producer
log maps-feature-replay-full-host-20261023.log. Pushed exactly those two commits
to codex/map-quality-delivery and verified the remote ref equals
2133547bf8e003e2f4c52de3b8abf737261a8397. No main merge, tag or release.
Requested incremental CodeRabbit review on PR #7 after the substantial push;
triage of review findings retains its pending approval.

Updated the consumer manifest to that commit and ran:

```text
python .local/maps/build_feature_replay_20261023.py update-dependencies
```

Exit 0. The component manager regenerated dependencies.lock; only the OrcMaps
revision/component hash and overall manifest hash changed. The existing build
gate confirms requested, locked and fetched revisions all equal 2133547.
Log: maps-feature-replay-native-update-dependencies-20261023.log. No hand edits
to the lock or managed component sources; other dependencies remain unchanged.
Pre-resolution sdkconfig, CMakeCache and lock snapshots are preserved under
.local/maps/maps-feature-replay-2133547-20261023. Native link and hardware gates
are still open at this checkpoint.

## Native candidate f38b8601 and COM17 results

Native build/inspection pass. Exact application SHA-256:
78d70808c018f46ede6023c082643c201810f2984cba6741fa63e446639ca138.
Size 5,268,960 bytes; factory headroom 1,022,496 bytes. Replay metadata is
2,360 bytes in .ext_ram.bss, plus its four-byte PSRAM buffer holder. Existing
40,960-byte DMA reserve, miniz ROM-symbol guard, shared FT8 PCM PSRAM placement,
saved SDK inputs, TEST-ONLY catalog and C6 3.0.6 image input pass inspection.
No decoder correctness test is claimed.

COM17 identity 30:ED:A0:E2:E6:95 was verified before access. The app-only flash
verified previous accepted app 30a00ce1, saved fresh NVS/PHY/coredump/world
backups, verified those backups, wrote only 0x10000, then verified the new app,
bootloader/table and all preserved partitions. C6 and SD files were not flashed.
Startup reaches BOOT_STAGE ready/SPLASH_READY. App/ELF/manifest and fresh backups
remain in .local/maps/maps-feature-replay-2133547-20261023. Rollback app 30a00ce1
is preserved separately. The native monitor was closed before command tests.

Exact commands from the integration root:

```text
python .local/maps/build_feature_replay_20261023.py build
python .local/maps/inspect_feature_replay_20261023.py
python .local/maps/flash_feature_replay_20261023.py
python .local/maps/com17_label_bench.py maps-feature-replay-warm-plan-20261023.json maps-feature-replay-warm-device-20261023.log
python .local/maps/com17_label_bench.py maps-feature-replay-measurement-plan-20261023.json maps-feature-replay-measurement-device-20261023.log
python .local/maps/summarize_feature_replay_20261023.py
python .local/maps/com17_label_bench.py maps-feature-replay-retained-sd-paused-plan-20261023.json maps-feature-replay-retained-sd-paused-device-20261023.log
python .local/maps/com17_label_bench.py maps-feature-replay-reclaim-plan-20261023.json maps-feature-replay-reclaim-device-20261023.log
```

All these commands exit 0. Build/inspection/flash logs use the
maps-feature-replay-native-* and maps-feature-replay-{verify-previous,backup-*,
verify-backups,app-write,preservation}-20261023.log prefixes. Startup log:
maps-feature-replay-boot-native-monitor-20261023.log. Console companions and
plans are preserved beside invocation. Tests below are separate from physical
appearance acceptance, which remains pending for this exact candidate.

### Fixed 20-pan performance gate: FAIL

Same routine 40,0 / 0,20 / -40,0 / 0,-20 pattern repeated five times, LoRa
reception 906.875 MHz, 960 kS/s, manual gain requested 421. Six source-zoom-8
tiles/5,763 features at requested zoom 9, labels off. Each status is ready with
a new generation and zero missing/failed tiles. Percentiles use nearest rank.

| Measured stage | p50 | p95 | Maximum |
|---|---:|---:|---:|
| Complete detail frame | 3.312016 s | 3.549776 s | 3.584369 s |
| Replay/decode/classification excluding timed drawing | 1.178463 s | 1.278304 s | 1.300130 s |
| Timed drawing/ordered flush | 2.085706 s | 2.199527 s | 2.238527 s |
| Tile-source IO | 0 | 0 | 0 |

All twenty pans hit all six replays, retaining 702,110 bytes in the 786,432-byte
arena, with zero tile reads and no fallback. The two-second p95 gate fails.
Summary/individual records: maps-feature-replay-measurement-summary-20261023.json.
These wall-time stages include scheduling/radio interference; they are not
isolated CPU timings or a causal speed multiplier versus the earlier baseline.
An initial frame was observed at 4.308048 s; no cold-SD bound is established.

During the measured run, driver rate is about 959 kS/s, drops=0/overruns=0;
logger ready=true,error=false,queued=0,dropped=0. Sample free PSRAM 898,868 bytes,
largest 851,968; largest DMA 19,456; main-stack minimum 1,064 bytes, unchanged
reset reason 11 and monotonic uptime. Worker-stack acceptance is not inferred.

### Retained SD, reclaim and fallback

The initial active-RF SD transfer attempt was rejected by SD_GET_ERROR radio_busy
while checking scratch absence. No upload was issued; the failed harness log is
maps-feature-replay-retained-sd-{device,console}-20261023.log. This is the
existing serial-transfer policy, not evidence of an SD-card read failure.

Paused reception, retained map/replay, then wrote/read 16,384 bytes at the
verified-absent owned path /orcsdr/maplabels_330606cb_feature_replay_20261023.bin.
Readback SHA-256 matches input:
a1f259d4365ed4320c377ce26f5c8c56dcdc9a89e7b641bfd8eabfbbeac86654.
Replay remained 702,110 bytes before/after; saved receiver point remained
configured and unchanged. Reception resumes at the same frequency/gain, about
952 kS/s in the immediate sample, with no drops/overruns. This does not prove
simultaneous RF transfer support; the scratch file is intentionally retained.

On Home, RTL_MEM RECLAIM releases exactly 2,160,016 bytes: two 555,720-byte map
buffers, 262,144-byte read cache and 786,432-byte replay arena. Map holder then
reports zero, while the active LoRa IQ guard keeps its buffers. Reentering LoRa
Map produces complete frames through ordinary streaming. The optional arena
does not reallocate because its conservative largest-block/headroom guard is
not met after fragmentation; capacity reports zero. This is a passing fallback,
not proof that optional replay can always be recreated after reclaim.

Final sample free PSRAM 1,684,788/largest 1,114,112; largest DMA 19,456; main-stack
minimum 1,064; streaming about 959 kS/s, zero drops/overruns and healthy logger.
COM17 remains f38b8601 on LoRa Map with replay currently disabled by the guard.
No monitor or command process remains open after the plans finish.

Next gate: reduce measured replay/drawing work without losing detail or starving
radio servicing. Do not lower the acceptance target, weaken allocation guards,
claim a full final review or declare completion based on the working cache alone.

## Next source-only candidate

Prepared producer 6a621a7 rejects wholly off-screen ordered geometry before
projection/primitive work, retaining conservative stroke margins and compatibility
with targets declining tile scissors. Three focused engine programs and a
sanitized paint-order program pass. Sixteen complete RGBA frames match producer
2133547 byte-for-byte, including actual FreeType labels, pans, Tokyo, antimeridian
and whole-world views. Reference primitive counts fall from 6,074 to 4,437 line
calls and from 6,176 to 1,881 polygon-ring calls. This is work avoided, not a P4
speed claim.

The consumer now also retains the cached classification instead of recomputing
it on every replay. Archive/profile invalidation still applies. Targeted Pipeline/
cache CTest passes 2/2 against the local producer source; P4 syntax checks pass
for Pipeline, map_service and renderer. Logs are maps-offscreen-integration-
20261023.log, maps-offscreen-p4-syntax-20261023.log and maps-offscreen-oracle-
{reference,candidate,comparison}-20261023.{log,json} in the integration root.

The producer change is committed locally but its new full-suite/publication
approval is pending. The manifest/lock still select 2133547, and COM17 still
runs f38b8601. Do not build or label a device artifact as containing the new
renderer until the approved producer is fetchable and the lock is regenerated.

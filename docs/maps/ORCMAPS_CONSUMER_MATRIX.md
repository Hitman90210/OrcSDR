# OrcMaps consumers and integration design


## Latest storage checkpoint: retained-map SD checks on COM39

Firmware `5c6f2853` builds/boots with the full 40 KiB reserve. Three 2.76 MB reads
and a unique 131,109-byte scratch write/readback pass with the map frame retained;
no reclaim. LoRa CSV readback adds two RF rows and preserves all 409 old rows.
Logging ends ready/error=false/dropped=0. Before flashing, old firmware already
reported error=true/dropped=33; those lost records are not recovered. Tight DMA
headroom, live SSTV/FT8 candidate checks and regional-SD concurrency remain open.
This is a bounded device result, not all-mode durability qualification or a soak.
COM17/C6 untouched; no push/merge. [SD results and logs](../evidence/maps/2026-10-17-sd-guard-device-result.md).
[Shared storage regression reference](../SD_CARD_RELIABILITY.md).

## Earlier display checkpoint: OrcMaps v0.2.3 and readable footer on COM39

Firmware `99affb1b` boots with OrcMaps v0.2.3 (`dc6bc12`) and the full 40 KiB
DMA reserve. Three device map crops match the previous renderer exactly; stored
location fields match. User confirms enlarged text is readable and controls good.
Loaded redraw samples take 1.41-2.42 seconds; return to the starting view takes
1.91 seconds, so no consistent speed multiplier is established. Driver drops and
overruns are zero at sampled checks; one candidate CRC-valid LoRa packet observed.
HOME's 33 ms setting yields p50 38/p95 43 ms in a short sample. Internal/DMA
headroom and held-map-memory SD reliability remain open. COM17/C6 untouched.
No push or merge; integration acceptance remains incomplete.

[Device results, commands and logs](../evidence/maps/2026-10-17-v023-device-result.md).
[Preceding host/build verification](../evidence/maps/2026-10-17-v023-upgrade-result.md).

## Previous device checkpoint: navigation and changed-location save tested on COM39

Normal firmware `04898891` (implementation `ef756048`) builds, flashes and boots.
Closer starting zoom, pan arrows, prominent map button and changed-location
save/display/reboot persistence pass the bounded COM39 checks. Original live point
restored. The user confirms controls and coarser world detail look good.

Active-LoRa redraw samples take 1.5-2.7 seconds, with zero sampled driver
drops/overruns. Final passive observation records 18 CRC-valid packets and nine
decoder failures (causes uninvestigated), not a controlled decode/soak qualification.
Held-cache SD read passes again; earlier intermittent SD/capture issues and tight
internal/DMA margins remain open. Regional/dashboard/recovery/release acceptance
is incomplete. COM17/C6 untouched; no merge or push.

[Device results, commands and logs](../evidence/maps/2026-10-17-picker-navigation-device-result.md).
[Preceding host verification](../evidence/maps/2026-10-17-picker-navigation-save-host.md).
Earlier sections are chronological evidence, not the latest completion claim.


Baseline: OrcSDR fba861e3, OrcMaps 14cdd41. Source is authoritative over stale README/STATUS text.

| Consumer | State/data owner | Current implementation/projection | Camera and overlays | Pack/format and evidence |
|---|---|---|---|---|
| ADS-B | ADS-B live/settings snapshots; canonical location copied through legacy settings | offline_map::draw_base on cached radar sprite; local equirectangular/NM basemap | receiver/range centered; aircraft use bearing/range polar placement | legacy `/orcsdr/data/lane_county_map.idx`, ORCMAP1; adsb_dashboard.cpp:106,120,1001 |
| LoRa | verified RF device/history snapshot | offline_map::draw_base/project | fixed 15 NM view centered on coordinate-bearing node; verified-coordinate markers | same legacy index; lora_dashboard.cpp:677,687,694 |
| FT8/FT4/JS8 | shared FT8 dashboard decoded grid/station snapshot | separate Natural Earth polygon array and equirectangular world projection | whole world by explicit existing preference; grid-square representative points | compiled asset, not PMTiles; ft8_dashboard.cpp:672-745; docs/ft8/CHANGELOG.md:134 |
| Airband | canonical receiver location through common Settings | no current map surface | location text/ATC selection; no independent camera | airband_dashboard.cpp:503 |
| Weather | no merged implementation | UNIMPLEMENTED on current main | PR #173 closed without merge | do not invent a current consumer |
| Location picker | no current implementation | old branch has prototype OrcMaps picker | blocking loop violates ownership/service requirement | historical setup_map_picker, not production |
| Setup wizard | no current implementation | old pure wizard/store model and blocking UI prototype | prototype skips welcome/network and auto-completes maps | not integrated on this baseline |

No other runtime map consumer found in the keyword/file audit. Satellite is not a current integrated map renderer in this baseline.

## Responsibility contract

OrcMaps owns PMTiles/MVT decoding, projection, viewport/camera, portable rendering and bounded scratch. OrcSDR owns receiver snapshots, canonical location persistence, signed downloads, screen transitions, and RF semantics. Current OrcMaps has no complete MapEngine/cache/overlay framework; build a thin shared map-session adapter over its public APIs, not a fork of its implementation.

Keep receiver location, map-pack selection, map-camera center, and approximate IP proposal separate. Projection used for basemap and markers must be identical after migration. Keep missing-coordinate aircraft/nodes unplotted, and label Maidenhead positions by their actual grid precision. Preserve FT8's whole-world preference.

One screen_controller owner; one radio_session owner. Maps may draw only on the owning surface or its off-screen target. Picker service is event driven; no touch polling loop. The first migration used LoRa; ADS-B followed in a separate commit. FT8/JS8 now call the shared whole-world compatibility adapter.

Do not copy old main.cpp, partitions or driver manifest. Consumer migration and P4 rendering remain UNTESTED until implemented and measured.

## Shared adapter foundation

`map_pipeline.*` now provides the common archive/reader/streaming-scratch/render path and E7-to-OrcMaps projection. It accepts a portable RenderTarget and cancellation/yield hooks; it contains no M5GFX, network, radio, touch or display ownership. Repeated wrapped tile placements decode once, then draw each placement. RenderStats distinguishes I/O, lookup, stream/decode and feature rendering; no P4 numbers are inferred from host timings.

`map_inventory.*` separately invokes OrcMaps discovery/pack resolution, preserves manifest metadata/attribution and explicit size-only integrity semantics, and rejects more than 16 manifests explicitly. The device filesystem adapter must also bound directory enumeration before allocation; host-only limits do not establish device safety. Signed catalog/P25 parsing is unchanged.

The candidate engine is reviewed PR #4 commit `70fea367909c74794bb69c7aed97e0f11f0a9387`, a descendant of v0.2.1, tested separately under MSVC AddressSanitizer. It is not claimed merged upstream or hardware-accepted. Dashboard/device integration is still pending.

### Reproduce foundation tests

From the repository root, with a checkout of the candidate engine:

```sh
cmake -S apps/orcsdr-tab5/tests/maps -B build-maps-tests -DORCMAPS_ROOT=/path/to/OrcMaps
cmake --build build-maps-tests --config RelWithDebInfo
ctest --test-dir build-maps-tests -C RelWithDebInfo --output-on-failure
```

On Windows this session used Visual Studio 18 2026, `/fsanitize=address`, and assertions explicitly enabled even in RelWithDebInfo. The test target passed projection/dimensions/antimeridian, repeatable pixels, cancellation, invalid archive/profile/zoom, 0/1/5/16 discovered packs, limit overflow, no SD, missing archive, mismatched size and malformed manifest cases. Log: `maps-pipeline-host.log` (invocation directory), reviewed copy in `docs/evidence/maps/`.

## Integrated callers (host-built; device gates pending)

| Caller | Shared entry point | Preserved semantics / remaining acceptance |
|---|---|---|
| LoRa | `maps::draw_region` / `maps::project(RegionalView)` | E7 verified positions only; follow camera stays separate from receiver location; ORCMAP1 fallback carries its matching projection |
| ADS-B | same region adapter into radar sprite | aircraft position and hit test use the basemap projection; missing coordinates and out-of-range tracks are not pinned to an invented edge location; radio list remains independent |
| FT8/FT4/JS8 | `maps::draw_world_compat` / `world_compat_project` | existing whole-globe equirectangular Natural Earth geometry centralized; full polar coverage and station-reported Maidenhead precision retained |
| Settings location / setup | `map_screen` + OrcMaps LocationPicker + shared worker | one proposal/confirmation flow; no network or SD prerequisite; manual polar E7 fallback |
| Airband | canonical receiver location, no new map | unchanged |
| Weather | absent on integration baseline | no implementation invented |

FT8 compatibility is deliberate, not a PMTiles engine claim: current OrcMaps provides
Mercator, which excludes polar grid locations, and its repeated-world placement would
change the owner's single whole-globe presentation. The existing compiled renderer and
projection now live in the shared map layer rather than each dashboard. A future engine
projection addition needs its own tests and upstream review before this path is retired.

`map_service` owns exactly one private PSRAM sprite and latest-request queue. It never
pushes a physical display from the worker; consumers request/push on the UI owner. Pan
queues/cancels before taking the frame lock, so the UI does not wait for rendering.
Memory reclaim invalidates the generation so the same viewport can be requested again.
Metadata rescans reopen the source/reader and publish a new inventory revision. The
frame cache and one PMTiles leaf cache are the actual caches; there is no invented
rendered-tile LRU, production MapEngine, or generic overlay framework.

Engine cleanup fix `c3cc09de3882149e03c7c00d7897c6882b324c6b` is under draft upstream
review at https://github.com/hardcoreerik/OrcMaps/pull/5, stacked on reviewed PR4. It
removes allocations during failed-tile cleanup. OrcSDR's prepared adapter catches
allocation failure and invalidates/reclaims the failed frame; host recovery tests pass.
The exception-enabled ESP32-P4 rebuild and all physical acceptance remain separate gates.

## Historical acceptance checkpoint — 2026-10-17

Current acceptance and unresolved release gates are tracked in the
[release continuation ledger](../superpowers/plans/2026-10-25-release-review-iq-continuation.md).

COM39 candidates `4971ee5b` and `e87c6d3c` failed before OrcSDR startup; none of the integrated
dashboard map consumers received a hardware acceptance pass. The original firmware
is restored and receiving LoRa. The placement adjustment reclaimed 592 internal BSS
bytes on the device but still failed boot. The approved startup diagnostic captured
an allocation alignment boundary, now reproduced with 32-bit SDK TLSF. Four map
metadata payloads are prepared for PSRAM placement; full build and device tests are
pending. Consumer performance/visual claims
below remain host/source results until the COM39 gate passes. Full details and logs
are in `P4_PERFORMANCE_EVIDENCE.md`.

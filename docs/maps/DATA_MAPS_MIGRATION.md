# Data & Maps inventory and migration


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


## Why Lane County appears

`catalog_sync.cpp:66-69` hard-codes five built-in identities/titles, including lane_county_map/LANE COUNTY MAP. Actual installed detection is separate (`refresh_installed`, line 163). COM39 reports that row uninstalled; COM17 reports it installed. A row is not proof of download.

The legacy path is `offline_map.hpp:10`, `/orcsdr/data/lane_county_map.idx`; header ORCMAP1 and bounded 640-segment/32-label cache are in offline_map.cpp:15-25,108. Legacy data remains discoverable and unchanged. Regional names remain legitimate metadata for actual regional packs.

PR #84's world ORCMAP1 asset was never merged. FT8's compiled world polygons are a separate firmware asset, not a shared installed regional pack. OrcMaps expects `/orcmaps/name.pmtiles` + `name.manifest.json` (+ optional checksum sidecar), PMTiles v3/MVT; no filename rename can convert formats.

## Cardinality and index safety

`catalog_sync.hpp:10-11` defines five built-ins and sixteen total slots. `settings_app.hpp:108` and drawing/touch loops expose only five. Slots 0..4 must retain existing IDs/command semantics. Dynamic p25_* profiles occupy later slots; arbitrary PMTiles IDs are currently not accepted by the signed catalog parser. Display all nonempty slots in five-row pages while sending original slot indices, never page-relative indices. Page changes and changed catalog identities clear destructive confirmations; invalid rows return no action.

The reviewed reconciliation branch supplies a small pure paging helper and targeted tests. Its independent contributions can be ported after review, retaining provenance. It does not supply PMTiles download/inventory.

## Truthful inventory states

Model separately: firmware/world overview; installed local pack; available signed offer; unavailable/no SD; malformed/partial/unknown legacy data; offline/unverified catalog. Show known identity, region/bounds, zoom, version, bytes, attribution/provenance and integrity level. Size pairing is not SHA integrity. Never label firmware text as installed data, or claim a catalog was verified without signature validation.

Preserve signature verification, declared hashes, staging `.part`, rollback `.bak`, version sidecars and existing P25 index gates. No removal/overwrite of user history or legacy maps. New pack formats need explicit destination/format validation, not a relaxation of the existing catalog security boundary.

Required tests: zero/one/five/six/sixteen/sparse entries, page bounds and original indices, install/remove confirmation reset, P25 entries, offline/no SD, legacy state and corrupt manifests. No device download/removal/migration is authorized by the initial read-only audit.

## Catalog implementation stage

Individually reviewed source from reconciliation commits through `a6ed3763` was selectively applied for the generic legacy title, 16-slot Settings snapshot, and pager UI. A tested Cursor now centralizes install/remove eligibility and confirmation state; page/catalog changes clear confirmation. Capacity is derived from `catalog::kPackCount` rather than a second numeric constant. No main.cpp changes, catalog parser changes, or device writes are involved.

MSVC C++17 `/W4 /WX /fsanitize=address /Zi /Od` test passed zero/one/five/six/sixteen/sparse entries, dynamic P25 slot, busy/offline gates, invalid indices, and removal confirmation/reset. Log is `maps-catalog-paging-msvc.log` in the invocation directory, with a reviewed copy in `docs/evidence/maps/`. A WSL attempt failed at VM connection establishment (HCS_E_CONNECTION_TIMEOUT), not a code test failure. Firmware compilation and device visual acceptance for this change remain separate gates.

## Implemented local inventory and world provisioning

The signed download catalog and `/sd/orcmaps/*.manifest.json` discovery remain separate.
LOCAL MAPS enumerates metadata, identity, region/bounds, zoom, version, required credit,
provenance and storage bytes. It distinguishes PMTiles, the legacy ORCMAP1 file and the
optional PMTiles world partition. Size pairing is labelled separately from archive hash
verification; discovery is never an installed-SHA claim. At most 16 manifest names are
accepted; the directory scan stops at the 17th and reports overflow rather than hiding
additional entries. Malformed/missing/mismatched entries are rejected, not installed.

The optional Natural Earth 5.1.2 z0-z4 world archive is 871,343 bytes with SHA-256
`9aea08772bacf1f024d1da90cc52aa8fcf0b0e37405415dc7c91e75e56596f0f`.
Its existing local builder output and manifest were inspected and hashed. It is public
domain (credit: Made with Natural Earth), with no OpenStreetMap data. The purported
v0.2.0 release asset named in upstream documentation was not found in current releases;
this session uses the verified local artifact, not an invented published download.

Build argument `-WorldMap <absolute archive path>` checks that exact SHA and includes
it in flash_args. An omitted argument clears the cached selection and does not provision
world data. The app has a coordinate-only fallback if the partition is missing/unreadable.
The new readonly 1 MiB `orcmaps_world` partition starts at 0x650000. NVS 0x9000,
PHY 0xf000, factory 0x10000/6MiB, and coredump 0x610000/256KiB are unchanged.
`maps-partition-layout.log` records decoding/verification of the actual built table.
Preparing these files does not flash a device. Target confirmation and preservation
of existing flash/NVS are required before any provisioning.

FT8's previously compiled Natural Earth outline is a distinct legacy world rendering
path, not evidence that a PMTiles world partition is installed. It remains available
without SD and keeps the complete globe/polar-locator semantics through the common
compatibility adapter. No ORCMAP1-to-PMTiles file conversion or SD rewriting is performed.

## Latest provisioning status — 2026-10-17

COM39's world archive write passed flash hash verification on both approved candidate
attempts. Both failed before application startup, and the original partition table was restored each time.
World bytes remain in the previously unassigned region but are not exposed by the
running firmware's partition table. No SD map pack or existing SD file was replaced.
COM17 was not accessed. Discovery and compatibility acceptance on the new firmware
remain pending; see `P4_PERFORMANCE_EVIDENCE.md` for recovery evidence.

The subsequent approved `fa520a34` diagnostic verified existing world bytes rather
than rewriting them, then restored the original partition table again. Its startup
capture and host reproduction isolate an allocation alignment boundary. A map-state
placement adjustment is prepared; no installed-pack or migration acceptance passed.

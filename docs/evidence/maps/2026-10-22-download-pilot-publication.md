# Approved TEST-ONLY GitHub map delivery pilot

The user explicitly approved the prepared `8c0e3b6c` pilot: 543 MB candidate
upload, TEST-ONLY P-256 signed catalog and COM17 installation/recovery tests.
Production signing, paid services, main merge, erasure and COM39 access remain
outside scope.

## Published endpoints

- [Candidate data prerelease](https://github.com/hardcoreerik/OrcMaps/releases/tag/sha256/16adde0b4f67734535db6871b89949e68c7e1dabf1c34d2bd7bd23842a9e5991)
- [TEST-ONLY signed catalog prerelease](https://github.com/hardcoreerik/OrcMaps/releases/tag/maps-catalog-test-20261022)

Both target engine `b59d47819cc3b81c1bbe50ed75b775275bb0218e`, are prereleases
and are excluded from latest. The existing `world-overview-1` draft is unchanged.
Release IDs: data `421233819`, catalog `421235072`.

Ruling: use tag `sha256/<full archive hash>`. Git accepts a pure 64-hex tag,
but GitHub refuses publishing it (`pre_receive ... 40 or 64 hex characters are
not allowed`). The first publication therefore fails HTTP 422 with both releases
still drafts. Prefixing with `sha256/` preserves the downloader's complete-hash
path-segment rule. The catalog is regenerated and signed again before either
release is published; no validator exception or security workaround is added.

Archive: 543,380,195 bytes, SHA-256
`16adde0b4f67734535db6871b89949e68c7e1dabf1c34d2bd7bd23842a9e5991`.
Actual producer hashes the full staged archive and invokes a newly built real
engine verifier; header-coverage contract passes with one installed, zero rejected.
Staging uses a local hard link, not a second 543 MB copy. No archive mutation.

Catalog revision `20261022214257`, generated `2026-10-22T21:42:57Z`, expires
`2026-11-22T21:42:57Z`. Final catalog 3,060 bytes, SHA-256
`102ebc795f930073c7b618c14e5789d873ef41aa926190fbf1489e303ce9569a`.
Detached signature 97 bytes, SHA-256
`5b371b59055702153530b7a7a2d8ba32273eaa32ac40e9171081771b09b87f49`.
TEST-ONLY public PEM 178 bytes, SHA-256
`7b899b1d10f3e98c348fd7a87c3bba32b4ae1ee833c2993122af3a7979118405`.
Private material is local, ignored and never uploaded or embedded in firmware.

The manifest, checksum and attribution/source/license notes accompany data.
This is the documented capped Protomaps/OSM/Natural Earth extract; upstream full
planet hash and exact Natural Earth revision remain unverified. Publication is
not worldwide visual acceptance or production capacity acceptance.

## Executed commands and evidence

Commands invoked from the OrcSDR integration root unless otherwise noted:

```text
python .local/maps/prepare_approved_pilot_20261022.py
wsl -d Ubuntu -- bash -lc 'cd /mnt/f/AI/OrcSDR-TEMP/maps-integration-completion && python3 /mnt/f/AI/OrcSDR-TEMP/orcmaps-quality-delivery/tools/pack-builder/build_map_catalog.py .local/maps/pilot-20261022/catalog-input-github-tag.json --pack-verifier /mnt/f/AI/OrcSDR-TEMP/orcmaps-quality-delivery/.local/pilot-pack-verifier-20261022/orcmap_pack_verify --private-key .local/maps/pilot-20261022/TEST-ONLY-keys/TEST-ONLY-private.pem --verify-public-key .local/maps/pilot-20261022/TEST-ONLY-public.pem --out .local/maps/pilot-20261022/signed-github-tag'
python .local/maps/upload_approved_pilot_20261022.py
python .local/maps/upload_pilot_catalog_20261022.py
python .local/maps/publish_corrected_pilot_20261022.py
python .local/maps/probe_published_pilot_20261022.py
python .local/maps/check_published_catalog_20261022.py
```

The Windows verifier rebuild first fails because CMake 4.0.3 does not support
cached generator Visual Studio 18 2026. Separate WSL Release verifier build
passes without source changes. Engine-worktree logs:
`pilot-pack-verifier-{build,linux-build}-20261022.log`.

Root logs: `pilot-{staging,catalog-producer,catalog-producer-github-tag,data-upload,catalog-upload,publication,publication-corrected,tag-correction}-20261022.log`.
GitHub inventory digests and sizes match local archives/metadata before publishing:
`pilot-verified-draft-{421233819,421235072}-20261022.json`.
Postpublication state: `pilot-releases-after-publication-20261022.json`.

Actual archive URL passes two 128-byte HTTP 206 ranges, exact Content-Range and
Content-Length, same strong ETag and If-Range, with verified HTTPS and bounded
redirects. Only 256 body bytes downloaded:
`pilot-published-range-probe-20261022.json` and `.log`.
Public catalog/signature/key downloaded within strict bounds match local bytes;
detached signature verifies using the downloaded public key:
`pilot-published-catalog-check-20261022.log`.

## Firmware and remaining device gates

Source `627c9d86` includes host-tested larger node names and `c6ab1833` stable
Follow ID. Installed COM17 remains `7c48e358` until app-only validation completes.
The TEST-ONLY build explicitly supplies saved SDKCONFIG, existing C6 3.0.6 image,
diagnostics OFF, pilot URL and public PEM; it does not establish production trust.

The prior backed-up NVS has no map-delivery namespace (trust floor zero), checked
without printing unrelated NVS values (`pilot-prior-trust-inventory-20261022.log`).
A fresh read is still required before catalog acceptance. Never erase/reset trust;
record the accepted test revision and require a compatible later production
revision/key-migration plan.

### COM17 pilot firmware and initial delivery checks

```text
python .local/maps/build_pilot_20261022.py build test-pilot-627c9d86-20261022
python .local/maps/inspect_test_pilot_20261022.py
python .local/maps/flash_test_pilot_20261022.py
python .local/maps/check_fresh_pilot_trust_20261022.py
python .local/maps/com17_label_bench.py test-pilot-catalog-check-plan-20261022.json test-pilot-catalog-check-device-20261022.log
python .local/maps/com17_label_bench.py test-pilot-first-cancel-plan-20261022.json test-pilot-first-cancel-device-20261022.log
python .local/maps/com17_label_bench.py test-pilot-resume-cancel-plan-20261022.json test-pilot-resume-cancel-device-20261022.log
```

Native build and inspection pass. App 5,157,440 bytes, partition headroom
1,134,016; SHA-256 `e435eef301ca9b284c2cf11907fff530a29ab84dc8cb4f0012f6d61738983127`.
ELF SHA-256 `5f93611dc95a78a6cc37acdac5f7863caf2e8931f08f150ead94f0c7c7ad586b`.
Exact pilot public key/URL, PSRAM placement, symbol gates, diagnostics OFF,
existing C6 3.0.6 input, dependency pin and full 40 KiB DMA reserve pass.
Artifact directory `.local/maps/pilot-627c9d86-20261022/` includes manifest and
fresh non-app backups. Previous app `7c48e358` verifies; app-only write and
bootloader/table/NVS/PHY/world/coredump preservation pass. COM39 untouched.

Fresh NVS backup contains no map-delivery namespace; trust floor zero, verified
without printing unrelated entries. No trust erase/reset. COM17 boots with
matching ELF hash and SD/Wi-Fi/RTL Ready. Native boot log is saved in its invocation
directory: `apps/orcsdr-tab5/test-pilot-boot-native-monitor-20261022.log`.

Setup opens with reception IDLE and receiver location unchanged at E7
`440628866,-1230160273`. Real HTTPS catalog check completes with phase 6,
configured=1, ready=1, count=1; the global product is recommended and not installed.
Given the compiled key and checked payload, the accepted revision is
`20261022214257`; later direct trust readback remains a separate persistence gate.

First install reaches 5,718,016 reported bytes, then cancellation finishes at
phase 3/busy 0, installed=0. Retry progresses to 14,385,152 bytes; a second
cancellation also finishes phase 3/busy 0, installed=0. No unexpected reset;
setup remains responsive to authenticated status/actions. These checks establish
bounded cancel/retry progress, not final archive integrity or activation.

Logs in root: `test-pilot-{artifact-inspection,verify-previous,backup-*,verify-backups,app-write,preservation,preflash-device,catalog-check-device,first-cancel-device,resume-cancel-device}-20261022.log`;
build log `fit-order-native-test-pilot-627c9d86-20261022-20261020.log`;
`pilot-fresh-trust-inventory-20261022.log`. Shared recording-display changes also
pass the real FT8 dashboard regression (`ft8-readable-stub-regression-20261022.log`).

Full transfer is started with a bounded adaptive observer:

```text
python .local/maps/com17_pilot_download_watch_20261022.py test-pilot-full-download-plan-20261022.json test-pilot-full-download-device-20261022.log
```

At this documentation checkpoint the observer is live; no completed-download
claim. It polls authenticated status every 30 seconds, records memory and fails
on a terminal non-success state. Do not restart solely because an observation
window expires: inspect the existing process/device first.

## Remaining gates

### Completed install and corrected-app follow-up

The existing full-download observer exits zero at 15:28:17.607. Its final
status is phase 2 (complete), busy 0, installed 1, 543,380,195 bytes. This is
the production installer path after complete-file SHA-256/PMTiles validation,
activation, ownership persistence and journal finalization; not merely a byte
counter reaching the expected length. First observed verification is 15:16:15.175,
first observed completion 15:28:12.020: approximately 716.845 seconds including
polling dwell (about 32-second resolution). Two full-file reads occur on the
fresh staging/activation path. This long verification/activation cost is a real
delivery-performance gap, not map-frame timing. `pilot-install-timing-20261022.json`.

App `ef53789f` adds the targeted Maps progress flicker correction and is now
app-only installed on COM17. Build/inspection and partition preservation pass;
app 5,157,984 bytes, SHA-256
`2e3a6e501e28ecf5f5891653fffc7ef5b18345b4a473f6742be05b2e85600c5d`.
ELF SHA-256 `155f6179a66e9785af4f36d0f5f235dbe6f9e88a4207614829041d1b8c2ff1bd`.
Matching boot reaches SD/Wi-Fi/RTL Ready. Newly installed SD pack is preserved.
Artifact/recovery: `.local/maps/maps-progress-ef53789f-20261022/`.

Before this update, the fresh NVS backup's map-delivery blob passes its checksum
and records exact accepted revision `20261022214257`, accepted catalog hash
`102ebc795f930073c7b618c14e5789d873ef41aa926190fbf1489e303ce9569a`, owned
`global-regional`, and no pending install. Only that namespace/blob is decoded;
unrelated NVS values are not printed. Log `pilot-persisted-trust-check-20261022.log`.
After the update/reboot, rechecking the same signed catalog succeeds and still
reports the global pack installed. No trust reset or key replacement occurred.

The installed pack is explicitly selected through the shared inventory action.
Receiver coordinates remain E7 `440628866,-1230160273`. LoRa then renders camera
zoom 9 from source zoom 8, six tiles, zero missing/failed, 5,763 features and
778,621 source bytes. First detailed frame total 4,497,426 us; cached transfer
26,581 us; three city labels drawn, zero unsupported/overflow. This proves the
new regional source is used, not just an enlarged embedded z4 overview.
It does not prove worldwide visual quality or the full 33 ms interaction gate.

The saved native screenshot shows roads, water/land cover, boundaries and city
names. Larger node names are visible, but clustered nodes still overlap; record
that as an open readability gap, not accepted decluttering. Capture:
`.local/maps/pilot-20261022/installed-lora.bmp`, 2,764,854 bytes, SHA-256
`f3b6c6c8a84d9f6fc0e866b9dfc348e20717bc2cd8cee52548e00b5f893721f1`.
Capture writes while reception is running; subsequent hash-checked serial image
read pauses reception and retains the completed map; LoRa is resumed afterward.
Final driver observation about 958 ksps, zero driver drops/overruns. Earlier
global-map observation: free PSRAM 2,077,836, largest 1,998,848; largest DMA 8,192;
main-stack high-water margin 1,544. These are bounded checks, not a soak.

Commands run from the integration root:

```text
python .local/maps/build_pilot_20261022.py build maps-progress-ef53789f-20261022
python .local/maps/inspect_maps_progress_20261022.py
python .local/maps/flash_maps_progress_20261022.py
python .local/maps/verify_pilot_persisted_trust_20261022.py
python .local/maps/com17_label_bench.py maps-progress-installed-lora-plan-20261022.json maps-progress-installed-lora-device-20261022.log
```

All exit zero. The plan's initial Cancel is rejected because no map screen owns
the display after reboot; the plan does not require that no-op. Open List and
Select Pack succeed. Do not interpret every command as acknowledged success.
Logs: `maps-progress-{artifact-inspection,verify-previous,backup-*,verify-backups,app-write,preservation,installed-lora-device}-20261022.log`;
build `fit-order-native-maps-progress-ef53789f-20261022-20261020.log`;
native monitor in invocation directory
`apps/orcsdr-tab5/maps-progress-boot-native-monitor-20261022.log`.

### Fresh offline SD frames — COM17

From the integration root:

```text
python .local/maps/com17_label_bench.py pilot-global-offline-render-plan-20261022.json pilot-global-offline-render-device-20261022.log
python .local/maps/check_offline_evidence_20261022.py
```

Both exit zero. The checker verifies ordered Wi-Fi status, two distinct completed
offline generations/revisions beyond the baseline, positive SD reads/bytes, zero
missing/failed tiles, unchanged saved receiver coordinates, restored Wi-Fi and
streaming with zero reported driver drops/overruns. Saved checker output:
`pilot-global-offline-evidence-check-20261022.log`; device log and console companion
are beside the invoked plan in the integration root.

Generation 6/revision 4: 7,524,396 us detailed redraw, 2,322,085 us IO and 26,613 us
cached push. Generation 7/revision 5: 7,248,452 us detailed redraw, 2,377,552 us IO
and 27,945 us cached push. Both read 767,298 bytes in 400 reads with six tiles.
The test disconnects Wi-Fi before zooming out/in and verifies it stays disconnected
after both new frames, then reconnects the saved profile. This distinguishes
offline source access from displaying a stale pre-disconnection image.

Final health: free PSRAM 2,077,812 bytes, largest 1,966,080; internal free 19,631;
largest DMA block 8,192; main-stack high-water margin 1,096. Final driver about
960 ksps, no reported drops/overruns. This remains a bounded regional check, not
worldwide quality acceptance, an overnight soak or a 33 ms interaction result.

Controlled interrupted activation recovery, hardware insufficient-space/corruption
rejection, physical flicker/larger-label acceptance,
clustered-node decluttering and worldwide global-pack native quality remain open.
The full integration goal is not complete. TEST-ONLY catalog configuration is
local build input; do not accidentally inherit it into production/release builds.

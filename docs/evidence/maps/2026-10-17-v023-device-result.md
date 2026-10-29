# OrcMaps v0.2.3 and readable picker footer: COM39 result

Date: 2026-10-17, America/Los_Angeles.
Firmware source: `99affb1b4463b8438a0a3e2b939406e7434659a4`.
Engine: `dc6bc12b7df9fb3decd751709da733a386cf386e` (v0.2.3).
This closes the approved candidate flash/bounded-check cycle. Integration and
release acceptance remain incomplete; nothing was pushed or merged.

## Candidate, target and preservation

The previously verified native artifact was used without another build or source
edit. [Build and 36 host pixel comparisons](2026-10-17-v023-upgrade-result.md)
remain separate, historical preflash evidence. The pin and regenerated lock are
in `99affb1b`; enlarged instructions/attribution are in `73f84ce7`.
App SHA-256 `a862da09eb48d695c2653ddb8462784a35ccf88ead06179a0a15cc289b20f447`;
ELF SHA-256 `91b577c0e6c9532bce550894eb1d7ca1c5ca0f5706a5266add586b14e5585016`.
Boot reports ELF prefix `91b577c0e` and the unchanged full 40 KiB DMA reserve.
UI_SELF_CHECK reports am/fm/home/shortwave/keypad/editor all 1.

COM39 USB serial E8:F6:0A:E2:D6:03 was verified for each esptool operation.
Fresh opaque boot/table/NVS/core/full-app backups and SHA manifest are retained
privately under `.local/maps/com39-recovery/v023-picker-20261017/`. The app backup
matched the saved working 04898891 prefix; the partition table matched before
writing. Only app@0x10000 was written and verified. Subsequent verify_flash
matched boot, table, NVS, previous core and embedded world (five digests).
No format, partition-table/world replacement, SD pack modification, COM17 access
or C6 flash. Screenshots were intentionally saved to SD. Runtime journals may
write NVS; preservation is verified before boot, not a post-runtime byte claim.

## Display and stored location

PASS: three actual device views (start zoom 3, zoom 4, half-screen right pan at
zoom 3) compare byte-for-byte in x=16,y=72,width=1248,height=524. Each comparison
covers 653,952 pixels, with zero different pixels. This includes the overlaid
controls but excludes the map's bottom 44 rows, where larger attribution is an
intentional application change. Complete screens are therefore not claimed
identical. [Hashes and counts](2026-10-17-v023-device-pixels.json).

Both Receiver Location coordinate-field images are identical before/after the
flash. No Use Location/save operation was issued; the current live user point,
which differs from older session notes, was preserved. Earlier changed-point
save/reboot tests are documented separately and were not repeated here.
User physical acceptance: **"Text is readable; controls look good"** for enlarged
bottom-left instructions and map credit. Saved screenshot also shows all controls.

## Active-radio timings

COM39: Blog V3-class dongle, previously connected LoRa antenna, US LongFast at
906.875 MHz, requested 960 ksps. Baseline 04898891 used MANUAL 49.6 dB gain.
Candidate initially booted in AUTO; its first capture timings are retained in the
counter log but are not used as the matching-gain comparison. Final pass restored
MANUAL 49.6 dB, RTL AGC off, bias off. Rendering used the same embedded world,
1248x568 viewport and unchanged world source-level policy.

| View | Baseline total ms | v0.2.3 total ms | Baseline render ms | v0.2.3 render ms |
|---|---:|---:|---:|---:|
| Starting zoom 3 | 1918.700 | 1410.425 | 1465.207 | 915.756 |
| Zoom 4 | 2151.907 | 2062.446 | 1350.640 | 1171.946 |
| Right pan, zoom 3 | 2491.907 | 2424.443 | 1929.112 | 1694.485 |

The candidate's return-left render of the starting view took 1909.734 ms total,
1438.037 ms render, showing substantial run variation. These are individual
loaded-device samples, not randomized/repeated benchmarks or a proven overall
speed multiplier. Total_us is the worker's measured interval; push_us is recorded
separately, not a complete button-to-visible latency measurement. UI waits were
5 seconds, not a quantitative touch-latency test.

All sampled views: missing=0/failed=0, frame buffer 1,417,728 bytes. Matching
views retain tile/feature/read counts (start 9/413/76, zoom 6/690/88, pan 12/715/106).
Final display transfers took 68.8-70.9 ms. `cached_pushes=1` is recorded, not proof
of a generic rendered-tile cache. No cache architecture or RenderScratch change.

Final matched-gain driver counters increased 14,876,672 -> 96,239,616 bytes,
effective sample rate 952,289 -> 958,619 sps, with drops=0/overruns=0 at both checks.
Candidate's earlier AUTO pass decoded one CRC-valid encrypted LoRa message, no
decoder failures/drops; the final manual pass received no additional decoded
packet. Natural RF was uncontrolled. This proves sampled stream continuity and
one candidate decode, not controlled RF sensitivity or audio/IQ-loss qualification.
No audio test was performed. No spontaneous reboot/panic observed in this bounded
capture; reset_reason=11 followed the deliberate USB reset, not an observed crash.

HOME -> RF Lab -> HOME console navigation passed. HOME sample: cadence setting
33 ms, 186 frames in 7108 ms (~26.2 updates/s); rolling 128 intervals p50=38 ms,
p95=43 ms; draw_max=32.838 ms. This is one short sample, not a new soak result.

## Memory and SD limits

Final map-visible/LoRa-streaming memory, bytes:

| Pool | Free | Largest block |
|---|---:|---:|
| Internal | 15,019 | 10,240 |
| Internal DMA | 1,175 | 832 |
| PSRAM | 2,045,344 | 1,998,848 |

Internal minimum was 807 bytes, DMA minimum 191; main stack high-water 3544.
These margins remain tight. No stronger heap-safety claim follows from boot/pass.
After screenshot creation, reception was stopped and RTL_MEM RECLAIM explicitly
released the 1,417,728-byte map frame before downloading four BMP files. All four
SD downloads passed byte count/SHA verification. This does not close the earlier
intermittent SD failures while holding map memory. No new pack-discovery/install
or map-cache-held SD stress test was performed.

Final device state: map picker at starting point/zoom, LoRa streaming, manual gain
49.6 dB. Harness exited and released COM39. COM17 and C6 untouched.

## Commands, logs and next gate

All commands ran from `F:/AI/OrcSDR-TEMP/maps-integration-completion/` with
`C:/Espressif/python_env/idf5.5_py3.14_env/Scripts/python.exe`:

```powershell
python .local/maps/com39_bench.py .local/maps/v023-baseline-plan.json maps-com39-v023-baseline-20261017.log
python .local/maps/flash-v023.py
python .local/maps/capture_nav_boot.py maps-com39-v023-boot-20261017.log
python .local/maps/com39_bench.py .local/maps/v023-boot-check-plan.json maps-com39-v023-boot-check-20261017.log
python .local/maps/com39_bench.py .local/maps/v023-candidate-plan.json maps-com39-v023-candidate-20261017.log
python .local/maps/compare_v023_device_pixels.py
python .local/maps/com39_bench.py .local/maps/v023-final-plan.json maps-com39-v023-final-20261017.log
```

All listed operations completed successfully. Private scripts use existing
authentication, no PAIR. Flash helper executes esptool read_flash backups,
write_flash app and verify_flash preservation checks; serial target is asserted.
Root logs are named `maps-com39-v023-{baseline,backup-app,backup-boot,backup-table,
backup-nvs,backup-core,flash,preservation,boot,boot-check,candidate,final}-20261017.log`.
Pixel results: `maps-com39-v023-pixel-comparison-20261017.log/.json`.
Reviewed coordinate/network-free numeric records:
`maps-com39-v023-reviewed-counters-20261017.log`, copied beside this report.
Device manifest: `maps-com39-v023-device-result-20261017.json`, also copied here.
Raw logs/screenshots remain private; the original preflash manifest stays unchanged.

Remaining blockers: tight internal/DMA headroom; held-map-memory SD reliability;
regional PMTiles discovery/pack-management and dashboard acceptance; interrupted
setup/offline fallback/recovery and release gates not fully qualified. This cycle
does not reopen the deferred Wi-Fi crash investigation. Next integration milestone
is qualify regional PMTiles discovery and shared dashboard maps against preserved
legacy data, addressing held-map-memory SD reliability before install claims.
No additional build/flash/test loop is implied by this report.

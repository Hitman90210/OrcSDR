# Clean GitHub map installation baseline — October 24, 2026

**Result: installed successfully; controlled pause/resume and native reboot/offline
rendering passed.** This is one mixed-load observation, not a statistical
performance guarantee. It is retained as a baseline for later improvements.

## Measured time

| Phase | First-observed duration |
|---|---:|
| Transfer window, excluding observed pause | **19m 40.3s** |
| Observed paused interval before Retry | **1.1s** |
| Verification and activation | **21m 21.2s** |
| Install request → observed installed result | **41m 02.6s** |

Average transfer-window rate: approximately **0.460 decimal MB/s**. This includes
queue/TLS/retry, SD writes and checkpoint overhead; it is not isolated network
bandwidth. Stop-request to observed paused state was 1.15 seconds, included in
the transfer window. Setup, catalog preparation, reboot and offline follow-up
are outside this timed install.

Timing uses Python's monotonic clock; local timestamps are Pacific daylight time:
request **11:37:05.757**, first observed verifying **11:56:47.204**, first observed
installed **12:18:08.370**. Status is sampled approximately every five seconds
with occasional health-query overhead. Actual verification start falls between
elapsed 1179.617–1184.663 s; completion between 2460.749–2465.829 s. The resulting
verification-duration bounds are **21m 16.1s–21m 26.2s**, not millisecond accuracy.

## Reproduction conditions

- COM17 Tab5 USB serial `30:ED:A0:E2:E6:95`; P4 source `137e244a`, OrcMaps `2133547`.
- PNY **32 GB microSDHC U1**, reported by the user. Controller: SD32G/SDHC,
  61,071,360 sectors ×512 bytes, 4-bit SDMMC at 40 MHz. Allocation unit unmeasured.
- Configured CPU 360 MHz, FreeRTOS 1000 Hz, hardware SHA enabled; 40 KiB internal
  DMA reserve. Download buffer 16 KiB, cache-aligned PSRAM. No settings changed
  during the measurement to tune speed.
- Existing TEST-ONLY signed GitHub catalog and the
  [global release asset](https://github.com/hardcoreerik/OrcMaps/releases/tag/sha256/16adde0b4f67734535db6871b89949e68c7e1dabf1c34d2bd7bd23842a9e5991).
  Archive **543,380,195 bytes**, SHA-256
  `16adde0b4f67734535db6871b89949e68c7e1dabf1c34d2bd7bd23842a9e5991`.
  HTTPS bytes went directly from the release service to the device, not via PC
  serial file copying. Device trust/location and other NVS state were retained.
- Fresh spare card: after remount, inventory contained only the firmware overview;
  catalog entry reported installed=0. The original populated card was kept safe.
  This is a clean map install, not an erased-device first-boot test.

The initial post-swap check saw stale inventory and SD read timeouts. No map
download had started. One controlled restart mounted the spare card successfully;
that preparation and its failed-read log are preserved separately from timings.
No erase/format command, firmware flash, C6 update or COM39 access occurred.

## Resume and workload changes

The observer requested cancellation after passing 32 MiB. Paused status reported
**37,240,832 bytes**; after Retry, the first positive progress sample was
**39,370,752 bytes**, followed by completion. The counter temporarily reported
zero while Retry initialized: a UI issue to improve, not a new full transfer.
The existing client validates resume identity/response and the whole final file;
no independent wire-level HTTP trace was collected. Power-loss resume was not
tested by this controlled pause/retry.

**The entire run must not be treated as idle-only.** Reception was stopped during
transfer. The user confirmed progress/controls responded, then left setup for
Home while the download continued. A read-only web snapshot at 11:52:47 confirmed
reception still stopped. At **12:06:03.938**, the log reports reception starting:
verification had run approximately **9m 16.7s** before that event and **12m 04.4s**
afterward. IQ capture starts also appear. The transition's cause was not inferred.

End-of-install driver snapshot: approximately 959 ksps, zero drops/overruns.
Measured installer log contains zero SD read/write error lines. Minimum sampled
PSRAM free 3,156,692 bytes; largest DMA block minimum 17,408; main stack HWM minimum
4,520. These are sampled values, not proof against every transient. No decoder
quality claim. Active reception alongside the HTTPS transfer was **not** tested;
the overlap occurred during verification/activation.

The prior pilot observed roughly 12 minutes post-transfer. This run used another
card and includes a receiver-load transition, so the difference cannot establish
an isolated firmware/card effect. The fresh path performs full staging validation
and a fresh activation check; separate hashing, SD-read and scheduler costs were
not instrumented. Integrity checks were not bypassed to improve the result.

## Reboot and offline acceptance

After installed=1, native IDF monitor captured a controlled restart with matching
ELF prefix `07d674c65`; SD/Wi-Fi/receiver reached Ready. The pack was discovered
again. Wi-Fi was explicitly disconnected, the regional pack selected, and the
picker zoomed to 6. Native result: regional title, source zoom 6, completed frame,
18 tiles, 598 features, 528 reads / 888,626 bytes, zero missing/failed tiles.
This used SD regional data, not the firmware overview or a pre-reboot frame.

The user saved a location during interactive setup at 11:49:51.756, recorded as
`ORC_MAP_LOCATION_SAVE OK verified`. No automated SAVE command was issued; that
post-interaction location remained identical through the offline follow-up.
The test did not overwrite it with the earlier preflight point.

Three driver drops/overruns were observed in the **post-reboot follow-up**, unlike
the zero-count end of the timed install. They are not attributed or hidden by an
all-tests continuity claim. A verifier initially compared a monotonic log prefix
as though it were saved-location data; corrected parsing passed, with the failed
harness log retained. This was not a device coordinate failure.

COM17 was returned to Home, saved Wi-Fi reconnected and reception active around
958 ksps. Existing memory reclaim released temporary map buffers without deleting
the pack; free PSRAM returned to 5,171,632 bytes, largest block 2,555,904. Final
driver snapshot after restore/restart had zero drops/overruns. Physical progress
page acceptance is confirmed. The user also confirmed the offline map looks
complete with its controls visible.

## Durable data and exact run commands

- [Timing summary and event/boundary times](timing-summary.json)
- [481 status samples](progress.csv): elapsed seconds, local time, phase, busy,
  configuration/catalog flags and received/total bytes. Phases: 6 ready,
  7 downloading, 3 paused, 1 verifying, 2 complete. The Retry zero is retained.
- [Firmware/card/observer/interaction metadata](metadata.json)
- [Reboot/offline verifier result](offline-result.json)

From `F:/AI/OrcSDR-TEMP/maps-integration-completion`:

```text
python .local/maps/com17_clean_github_measured_20261024.py maps-clean-github-measured-plan-20261024.json maps-clean-github-measured-device-20261024.log
python .local/maps/summarize_clean_github_20261024.py
python .local/maps/com17_label_bench.py maps-clean-github-offline-plan-20261024.json maps-clean-github-offline-device-20261024.log
python .local/maps/verify_clean_github_offline_20261024.py
```

The timed observer, summary analysis and corrected offline verifier exited 0.
Raw logs/plans/scripts remain in the invocation directory and `.local/maps`;
summary JSON records raw-log hashes. Primary names are
`maps-clean-github-measured-{device,console}-20261024.log`,
`maps-clean-github-timing-analysis-20261024.log`,
`maps-clean-github-reboot-native-monitor-20261024.log`,
`maps-clean-github-offline-{device,console}-20261024.log`,
`maps-clean-github-offline-verification{-corrected}-20261024.log`, and
`maps-clean-github-{restore,cleanup}-device-20261024.log`.

Next improvements to evaluate: numeric verification progress, preserving the
visible count during Retry initialization, and a persistent background-download
indicator with a return-to-progress action. Get Maps progress is already visible
and physically accepted; leaving it currently hides that status. Downloads and
new map rendering share a worker, so smooth map use during transfer is not proven.
Profile before changing the two verification passes; compare future runs with
card, radio/SD activity, UI, network and observer conditions held consistent.

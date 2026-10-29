# Framebuffer cache-writeback trial

Not a release pass. The experimental option has been removed from shipping
defaults because radio losses remain. No driver, decoder or engine change was
made in this trial; the earlier verified in-place LoRa correction remains.

## Identity and configuration

COM17 `30:ED:A0:E2:E6:95`, Blog V4 / driver 0.9.3, indoor 915 MHz whip,
US LongFast slot 20 at 906.875 MHz / 960 ksps, manual gain 49.6 dB,
RTL AGC off, bias off. COM39 was not used or flashed for this trial.

A2 app: source 5e79fd44; SHA-256
`773381897b679558feaf2ca912544b65728c2a0579a421d172891de3aad48938`.
B app: source 55957d67; SHA-256
`a19a2546bd9b5b218706c9bad46e4fde68da077acc07c4d844c9a6b9695cfb38`;
5,298,512 bytes / 992,944-byte partition headroom.

Kconfig probe and actual native header comparison both verified exactly two
macro differences: enable ESP_MM_CACHE_MSYNC_C2M_CHUNKED_OPS and use its P4
default MAX_LEN 0x20000. The 40 KiB DMA reserve, SDK, C6 image input, dependencies,
production catalog and symbol/PSRAM-placement gates remained unchanged.
Native build, app-only flash, backups/non-app preservation and boot passed.

## Measured result

Same script: idle, open picker, three alternating 40-pixel pans, cancel.
Both idle windows recorded zero additional driver and application drops.

| Map activity | A2, original configuration | B, chunked writeback |
|---|---:|---:|
| Driver drop increase | 55 | 14 |
| Application IQ queue drop increase | 1,101 | 472 |

Driver-delivered samples remained approximately 951–959 ksps during B map
phases, but application losses persisted. Some A2 requests were still busy at
their snapshot while B completed, so work completed was not identical; do not
turn these counts into a generalized speed claim. FM at 2.4 MSPS and physical
appearance checks were not completed. No decoder-by-dongle qualification follows
from this test.

With the B image, a uniquely named 8,192-byte scratch file was written and read
back with map memory retained (711,896 bytes free / largest 212,992). SHA-256
matched: `dc404a613fedaeb54034514bc6505f56b933caa5250299ba7d094377a51caa46`.
Reception was explicitly stopped for protected serial file transfer, then
restored to LoRa. This does not claim simultaneous RF and serial SD writes.
No existing SD file was overwritten; the scratch file is
`/orcsdr/maplabels_330606cb_cache_writeback_20261025.bin`.

## Evidence and remaining work

Logs/artifacts are outside Git, beside invocation in this worktree:
`cache-writeback-{A2,B}-device-20261025.log`,
`cache-writeback-ab-result-20261025.json`,
`cache-writeback-native-{build,inspection}-20261025.log`,
`cache-writeback-flash-console-20261025.log`,
`cache-writeback-boot-native-monitor-20261025.log`,
`cache-writeback-sd-device-20261025.log`,
`.local/maps/cache-writeback-55957d67-20261025/manifest.json`.

After the trial, COM17 was restored app-only to the verified 5e79fd44 image
(SHA-256 `773381897b679558feaf2ca912544b65728c2a0579a421d172891de3aad48938`).
Current-app verification, fresh backups, non-app preservation and startup passed.
The post-restore check reported two CRC-valid packets / zero decoder failures,
zero driver drops, approximately 5.13 MB free PSRAM and a 5.11 MB largest block.
Reception was restored to US LongFast slot 20 and the same manual 49.6 dB gain.

The original cached SDK input was restored and official IDF reconfiguration
passed; every generated configuration macro matches the original baseline.
The build directory still requires a rebuild before its binary outputs may be
used: reconfiguration is not relinking. The frozen restoration artifact is
`.local/maps/restore-lora-5e79fd44-20261025/manifest.json`.
Logs: `restore-lora-flash-console-20261025.log`,
`restore-lora-boot-native-monitor-20261025.log`,
`restore-lora-check-device-20261025.log`,
`cache-writeback-native-restore-inputs-20261025.log`.

The next combined firmware build must use shipping defaults.
Investigate long scheduling/cache stalls and unnecessary redraws using both
driver and application counters. Do not reduce the release gate to driver
counters alone. Required review findings and the release matrix remain open.

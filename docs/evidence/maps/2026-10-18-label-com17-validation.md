# COM17 label candidate — bounded hardware evidence

Target: COM17, USB serial 30:ED:A0:E2:E6:95. COM39 untouched.
Candidate app SHA-256: 6cdb02ce1a0c89afca2f031c31305edc9547ee2c705c7d017d14e607fca34306.

Previous app matched saved 81e9147d candidate exactly, ELF hash
168266eff5b1800cbd339fbb1361d0723002371876fee78aa8c04edc7468ef76.
Full 16 MiB backup read and verified before writing:
`.local/maps/com17-recovery/20261018-155834/` (manifest and partition copies).
App-only write and verification passed. Bootloader/table/NVS/PHY/coredump/world
verified unchanged before boot. C6 remains 3.0.6. No SD packs were overwritten.
User separately approved serial test pairing; PAIR_OK with the existing local
automation key, never logged. This is an authorized NVS pairing change after
the preservation check, not a claim NVS remains byte-identical afterward.

Commands used the identity-checked local scripts:
`.local/maps/com17_backup_inventory.py`, `flash_com17_labels_20261018.py`,
`capture_com17_boot.py`, and `com17_label_bench.py` with named root JSON plans.
Logs are in the integration worktree where the scripts ran:
`com17-label-preflight-20261018.log` (authentication failed; no writes),
`com17-label-public-20261018.log`, `com17-label-backup-run-20261018.log`,
`com17-label-app-write-20261018.log`,
`com17-label-app-and-preservation-verify-20261018.log`,
`com17-label-boot-20261018.log`, `com17-label-map-20261018.log`,
`com17-label-sd-idle-20261018.log`, `com17-label-nav-20261018.log`,
and `com17-label-setup-20261018.log`.

## Actual results

- Boot passed; SD/Wi-Fi/RTL ready. Driver 0.9.3 and C6 3.0.6 confirmed.
- Native map during reception: 9 tiles, 536 features, 13 drawn labels,
  3 unsupported names counted. Captured image contains actual readable labels.
  Physical panel acceptance is not established by the capture.
- Two HOME/RF Lab/HOME/Settings navigation cycles and four pan/zoom actions
  completed. UI self-check reports all six checks true. Final RF approximately
  2.4 MSPS, driver drops=0, overruns=0. Tested with attached V4; this is stream
  continuity, not a signal/decoder acceptance claim.
- Uncached pan/zoom frames: 1.445–1.806 seconds. Display pushes: 76.5–78.4 ms.
  **33 ms fluidity is not achieved.** Label draw stage approximately 31–72 ms.
- Final PSRAM free 19,096,548; internal free 28,971; largest internal 12,800;
  DMA free 8,455; largest DMA 8,192. Minimum DMA during this boot 3,663;
  minimum internal 4,107. Memory headroom remains a concern, not closed.
- Retained 1,417,728-byte map frame throughout bulk SD checks. Screenshot
  2,764,854 bytes transferred with hash verification; unique 8,192-byte scratch
  upload/readback matched byte-for-byte, SHA-256
  dc404a613fedaeb54034514bc6505f56b933caa5250299ba7d094377a51caa46.
  Scratch remains `/orcsdr/maplabels_330606cb_20261018_1605.bin`.
  Bulk transfer during RF correctly rejected radio_busy; successful transfers
  used paused reception. Unsupported RTL_LISTEN SHORTWAVE did not resume RF;
  corrected test opened Shortwave dashboard then tuned 14.070 MHz successfully.
- Wizard rerun opened and reached its own Network page without a Settings jump.
  Setup test stopped when a 4-second screenshot wait expired. It does not prove
  a hung device or complete wizard acceptance. User input overlapped this period;
  do not attribute all viewport changes to scripted actions.

## User-reported Use Location display defect

User photo shows full map-pack path spilling left over the Settings rail.
`draw_location` used unbounded right-aligned value_row text. Source fix limits
the displayed pack summary to 42 characters/bytes with ellipsis and avoids
splitting UTF-8 sequences. Stored pack identity is unchanged.
Focused regression added to map_screen_tests: 200-character identity first
fails assertion, then passes after fix; existing changed-coordinate save/return,
failure/cancel and Settings refresh tests also pass.

Exact test: `cmake --build .local/maps/labels-host --target orcsdr_maps_tests -j4`,
then `ctest --test-dir .local/maps/labels-host -R '^map_pipeline$' --output-on-failure`
under WSL. Red/green logs: `location-pack-text-red-20261018.log`,
`location-pack-text-green-build-20261018.log`, `location-pack-text-green-20261018.log`.
**1/1 passed. Fix not yet built/flashed.**

Remaining: native correction/physical acceptance, glyph reporting in picker UI
(FrameInfo status currently not drawn there), font coverage, cache/fluidity and
memory optimization, complete wizard/download/catalog and worldwide quality.

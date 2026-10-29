# Real verification feedback — prepared source

October 24 follow-up: [native COM17 build, preservation and navigation pass](2026-10-24-download-feedback-device.md), with physical header readability accepted. The source-only status below is the earlier checkpoint; real full-file numeric progress remains open.

The clean installation baseline spent 21m21.2s in verification/activation with
only an animated wait message. This change reports actual bytes checked and
the current scan number through Get Maps and `RTL_MAP DOWNLOAD LIST`.
It does not remove a read/hash/format check, cache a successful hash, skip
activation, alter pack formats or change the engine dependency.

## Change boundary

- `map_transfer.hpp/.cpp`: optional worker callback starts each full-file hash
  scan at zero and advances after successful read/hash updates. Final hash and
  PMTiles validation still decide success. Cancellation remains checked.
- `map_downloads.hpp/.cpp`: publish checked bytes/total/pass through the existing
  protected snapshot, throttled to 100 ms except first/final updates. New jobs
  reset those fields. No UI drawing from the worker.
- `map_screen.cpp`: display checked bytes, percent and pass number. Actual scan
  progress fills the existing bar; preparation without known size keeps the
  spinner. Value/pass updates repaint only the progress region, preserving
  static controls. It does not label 100% checked as Installed.
- `map_serial.cpp`: expose the same values as `verified`, `verify_total`,
  `verify_pass`; existing read-only authentication behavior is unchanged.
- Recording-display and real transfer tests cover progress, serial parity,
  cancellation and integrity rejection after checking every byte.

The P4 compiler confirms Snapshot size 152 bytes, previously 128: +24 bytes
per copy. The fixed worker/UI snapshots keep their EXT_RAM_BSS declarations
(48 extra bytes across those two copies); local copies also grow by 24 bytes.
No new sprite, framebuffer or bulk allocation. Native linking/placement and
loaded stack/memory measurements remain pending; declarations and syntax do
not establish the complete firmware budget.

## Exact checks and results

From `F:/AI/OrcSDR-TEMP/maps-integration-completion`:

```text
wsl bash -c 'cd /mnt/f/AI/OrcSDR-TEMP/maps-integration-completion && cmake --build .local/maps/feature-replay-integration-20261023 --target orcsdr_maps_tests -j2 && ctest --test-dir .local/maps/feature-replay-integration-20261023 -R "^map_pipeline$" --output-on-failure'
cmake --build .local/maps/catalog-security-20261019 --config Debug --target map_transfer_test
ctest --test-dir .local/maps/catalog-security-20261019 -C Debug -R '^map_transfer$' --output-on-failure
python .local/maps/check_verification_progress_syntax_20261024.py map_downloads.cpp map_transfer.cpp map_install.cpp map_serial.cpp map_screen.cpp main.cpp
python .local/maps/check_verification_snapshot_size_20261024.py ../../../.local/maps/progress_snapshot_size_20261024.cpp
```

UI selected test 1/1 passed. Updates through 25/50/75/100%, then pass 2,
preserve full-screen-clear count; serial fields match the UI snapshot.
Transfer selected test 1/1 passed: monotonic actual checked offsets, final total,
read-only cancellation, and a corrupt archive rejected despite 100% checked.
All six P4 syntax compilations and the size assertion exited 0.

Logs saved beside invocation:
`maps-verification-progress-ui-20261024.log`,
`maps-verification-progress-transfer-20261024.log`,
`maps-verification-progress-integrity-20261024.log`,
`maps-verification-progress-p4-syntax-20261024.log`,
`maps-verification-progress-p4-size-20261024.log`.

## Remaining gates

No build, flash, new large download or device operation for this source change.
COM17 remains on the physically accepted benchmark app `137e244a`; COM39
untouched. Native worker publication timing, final ELF/stack budgets, readable
progress on the physical display and verification overhead are untested.
Combine the next native validation with the remaining downloader work rather
than repeating the entire 543 MB benchmark for this edit alone.

The [background indicator/return-to-progress action](2026-10-24-background-download-notice.md)
now has focused host and P4 syntax coverage; native/physical validation and Retry
initialization counter continuity are still open. Downloads and fresh map rendering still
share a worker; this progress change does not establish simultaneous smooth
map rendering or active reception during HTTPS transfer.

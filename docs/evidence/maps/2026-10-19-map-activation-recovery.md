# Map activation and recovery — October 19, 2026

Scope: source/host validation, not device installation or SD power-loss acceptance.
COM17 and COM39 were not accessed. Existing device app remains the previously
verified `aca08b75` candidate; this transaction is not deployed.

## Behavior

The serialized installer rehashes staged bytes through the shared storage
wrapper and validates the actual PMTiles reader/header/root before activation.
An immutable, checksummed activation intent is synced before visible changes.
New files use archive-and-manifest hashes in their names. Previous metadata
moves into the hidden job as `previous.bak`; previous archive bytes remain intact.
The final manifest becomes visible only after retirement of the previous one.
A synced temporary commit marker is renamed to mark successful activation.

Recovery rolls incomplete activation back or checks a committed activation.
It refuses conflicting metadata, preserves damaged journals for attention and
quarantines a torn initial journal only when no mutation outputs exist.
Finalization removes the pending marker only after the caller records selection
and ownership durably; it retains prior metadata/archive recovery data.

The map worker now recovers pending jobs before inventory discovery. An SD I/O
error is distinct from a missing file. Failed recovery withholds SD PMTiles
discovery, adds an explicit inventory error and retains the embedded world and
legacy compatibility paths. Enumeration is bounded to 4096 entries and 64 pending
jobs; completed/finalized stages do not consume the pending-job allowance.

## Actual checks

Commands run from `F:/AI/OrcSDR-TEMP/maps-integration-completion`:

```powershell
cmake --build .local/maps/catalog-security-20261019 --config Debug --target map_transfer_test
ctest --test-dir .local/maps/catalog-security-20261019 -C Debug -R '^map_transfer$' --output-on-failure
python .local/maps/check_map_transfer_syntax_20261019.py map_activation.cpp map_service.cpp orcsdr_storage.cpp map_transfer.cpp
cmake --build .local/maps/timing-host-20261019 --config Debug --target orcsdr_sd_dma_tests
ctest --test-dir .local/maps/timing-host-20261019 -C Debug -R '^sd_dma_buffer$' --output-on-failure
git diff --check
```

- Transfer/activation CTest: **1/1 PASS**, including five replacement mutation
  boundaries and four first-install boundaries, retry/finalize idempotence,
  status errors, torn/corrupt journals, path rejection, conflicting external
  metadata, staged-byte corruption and preservation of unmanaged/previous maps.
  The fixture is an actual gzip PMTiles archive padded to about 2 MiB; network
  and filesystem faults are simulated. It does not exercise physical FAT/SDMMC.
- SD DMA buffer CTest: **1/1 PASS**.
- P4 syntax: **4/4 PASS** using the existing SDK 5.5.4 compile flags.
  The activation source is registered in the native component, but no native
  link, startup, HTTPS transfer or hardware install was executed in this step.
- `git diff --check`: PASS (line-ending notices only).
- Read-only scoped subagent review: no remaining verified blockers. An initial
  lifetime limit on finalized stage directories was corrected to count only
  pending journals before the final review.

Logs in the invocation directory:
`map-activation-build-20261019.log`, `map-activation-cases-20261019.log`,
`map-activation-p4-syntax-20261019.log`,
`map-activation-sd-build-20261019.log`, `map-activation-sd-test-20261019.log`.

Earlier implementation probes caught a Windows fixture path-length problem;
the backup now uses the bounded job-local `previous.bak` name. No SD paths or
allocation limits were relaxed to address a host-only path limitation.

## Remaining gates

Boot recovery checks committed archive size and metadata; it does not rehash
hundreds of MB at every boot. Explicit activation/retry verifies the full hash.
Host mutation boundaries do not prove physical rename/fsync durability or cover
every possible torn FAT sector. Real COM17 recovery, memory/stack, retained-map
SD and active-radio checks remain required.

No caller yet durably records managed-product ownership/selection or schedules
installation/finalization on the worker. Stable product identity must distinguish
updates from manually imported maps; display names are not ownership evidence.
Catalog trust persistence, HTTPS validation, wizard controls/progress/retry,
downloads, worldwide map acceptance and final dashboard integration remain open.

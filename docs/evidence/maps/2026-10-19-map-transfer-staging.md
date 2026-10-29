# Resumable map transfer staging — source and focused tests

Files: `ui/map_transfer.hpp/.cpp`, `ui/map_http_headers.hpp`,
`ui/map_http_stream.hpp/.cpp`, the additive `File::truncate` method in
`ui/orcsdr_storage.hpp/.cpp`, native CMake registration and the focused
`tests/maps/catalog_security/transfer_test.cpp` target.

## Implemented behavior

The synchronous operation is intended for a serialized worker, never the UI,
IQ, decoder or audio callbacks. It accepts a pack previously authenticated by
`read_signed`, a selected mirror, current free-space estimate and a caller-owned
4–64 KiB buffer (PSRAM on P4). The initial size limit is 600 MiB. It requires a
strong server ETag and checks status, exact declared length, unencoded bytes,
Content-Range and resume identity using the existing shared response guard.

Files live under `/orcmaps/.stage-<archive-hash>-<manifest-hash>/`. The flat
engine discovery operation does not recurse into this directory. No installed
pack, selected map, receiver location or catalog revision is changed. Success
means **prepared hidden files**, not installation or activation.

Two alternating 250-byte checkpoint records bind total size, archive hash,
synced prefix and strong ETag; a SHA-256 detects damaged records. Data is synced
before its checkpoint. Recovery selects a valid checkpoint and shrinks only
the staging file's uncheckpointed tail, after validating the new HTTP response.
An empty restart removes stale checkpoint records for this same hidden job.
Cancellation/network failure commits the received prefix when possible. Short
writes/sync errors remain failures; they never publish a map manifest.

After download, the operation closes HTTP and hashes bytes read back through
the shared SD wrapper. It then uses the actual engine PMTiles reader to open
the archive/root directory and check claimed bounds/zoom. Only then does it
rename the staged archive and sync/rename its staging manifest. The worker
gets a verification callback so the eventual UI can distinguish download from
verification rather than sitting at 100% without explanation.

The ESP-IDF transport uses the certificate bundle, identity encoding, bounded
critical headers, Range/If-Range, five-second HTTP timeouts and at most four
HTTPS-only redirects. Redirect URLs are not logged. Duplicate critical headers,
overlong ETags and unsafe/downgraded redirect locations are rejected. This code
compiles but **has not sent a real request in these tests**.

`File::truncate` flushes before shrinking, refuses extension/unsupported large
lengths, calls the SDK-supported FAT/VFS `ftruncate` and restores the append
cursor. It changes no existing read/write/append/buffering/DMA-check method.
The implementation is syntax-checked, not yet tested on a physical card.

## Actual verification

The target initially failed to configure with the implementation absent. A
subsequent harness run exposed Windows text-mode conversion in its mock storage
adapter; the adapter now uses binary modes, matching the real storage wrapper.
The first P4 check also caught one misleading-indentation warning, which was
fixed before the final pass. No allocation limits were relaxed.

From the OrcSDR worktree root, using the already-configured focused project:

```powershell
cmake --build .local/maps/catalog-security-20261019 --config Debug --target map_transfer_test
ctest --test-dir .local/maps/catalog-security-20261019 -C Debug -R '^map_transfer$' --output-on-failure
python .local/maps/check_map_transfer_syntax_20261019.py
cmake --build .local/maps/timing-host-20261019 --config Debug --target orcsdr_sd_dma_tests
ctest --test-dir .local/maps/timing-host-20261019 -C Debug -R '^sd_dma_buffer$' --output-on-failure
```

Results:

- Transfer CTest **1/1 PASS**. The harness uses an actual gzip PMTiles fixture
  with bounded padding (about 2 MiB), real hashing/PMTiles code, simulated HTTP
  and binary host file operations behind the shared storage interface.
- Cases cover free-space rejection before HTTP, cancellation before start,
  interruption/resume, ETag mismatch with unchanged staging, uncheckpointed
  tail shrink, damaged-record fallback, partial writes/retry, cancellation and
  resume, read-back corruption, valid-hash/invalid-header rejection and recovery
  of an archive left ready before its staging-manifest rename. Old fixture-map
  bytes remain intact. Header/redirect policy checks are included.
- The harness checks its resolved absolute fixture directory against the fixed
  workspace `.local/maps/map-transfer-fixture-20261019` before recursive cleanup.
  It never accepts an arbitrary cleanup destination or device/card path.
- P4 syntax **3/3 PASS**: transfer, HTTPS transport and shared storage source,
  with existing SDK 5.5.4 build flags. This is not linking, flashing or booting.
- Existing shared SD DMA buffer regression **1/1 PASS**.

Logs are in the invocation directory:
`map-transfer-{red,configure,build,cases,ctest,p4-syntax}-20261019.log`,
`map-http-policy-red-20261019.log`,
`map-transfer-sd-dma-{build,test}-20261019.log`.

## Still required

Host file `sync` is simulated with stdio flushing; these tests do not prove FAT
directory durability, actual SDMMC/DMA allocations or sudden power-loss behavior.
The callback/SDK TLS flow has no host network emulator and needs bounded COM17
HTTPS/range checks. No new global archive copy/download was performed.

The activation transaction must still retire old manifests without duplicate
identities and recover interrupted replacement before discovery. Catalog fetch,
persisted accepted revision/hash, reset/discard controls, worker scheduling,
wizard progress/retry/cancel wiring, native build/link, memory budgets and
active-radio SD measurements remain open. Neither COM17 nor COM39 was opened,
reset or flashed; NVS, maps, world partition and C6 were untouched.

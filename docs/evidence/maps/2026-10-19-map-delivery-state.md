# Catalog trust and managed map state — October 19, 2026

Source and focused host/P4 syntax evidence only. Neither Tab5 was accessed,
reset or flashed. COM17's last verified app remains `aca08b75`.

## Persistent contract

The installer state is one bounded, versioned, checksummed blob in the separate
NVS namespace `orcmapsdl`, key `state_v1`. It records:

- Accepted signed-catalog revision and exact payload SHA-256.
- Up to sixteen explicit product-to-managed-manifest ownership entries.
- Selected product and one pending install/selection intent.

The decoder caps the blob at 8192 bytes and checks every field and checksum
before replacing caller state. Invalid/duplicate product IDs, paths outside the
content-addressed managed filename form, duplicate ownership and unknown
selection are rejected. Read errors/corruption do not become empty state and
are never overwritten by a normal save. Lower revisions and different payloads
at an accepted revision are rejected. A save verifies exact blob readback.

The NVS adapter bounds allocation, distinguishes missing keys/namespaces from
errors, checks read lengths, closes handles and propagates set/commit errors.
This does not add migration, erasure or access to Wi-Fi/location settings.
It is not hardware-backed monotonic storage or protection against deliberate
flash erasure; physical NVS durability and heap consumption remain unverified.

`accept_catalog` now loads prior trust, invokes the actual signature/schema/time
consumer with that policy, and persists new revision/hash before exposing the
catalog. Identical accepted catalogs are still authenticated/time-checked but
do not rewrite flash. Commit failure leaves caller output unchanged. The caller
must still supply its approved public key and genuinely trusted UTC clock.

## Stable product and interrupted-install intent

Signed catalog entries now require `product_id`, e.g. `global-regional`, matching
the bounded lowercase ASCII syntax on both producer and consumer. This remains
the unpublished v1 catalog contract: old local test payloads without the field
must be regenerated. Pack manifest/PMTiles formats and the pinned engine do not
change. OrcMaps host-tool change: `e50c6bd`; not pushed or published in this step.

The product ID stays stable across snapshot/hash/pack-ID changes. Updating a
managed product never infers ownership from display names, regions or a manually
imported file. Installed versions retain old archives through activation.

`plan_install` records intended product, target manifest and selection before
activation without replacing current ownership. A reboot can therefore retain
this intent once the worker saves it, without another catalog fetch.
`record_install` requires a matching prior intent and checks the intended
completion and updates ownership/selection while clearing the pending intent.
The worker must save that state before finalizing the activation journal.
These APIs are tested; their actual worker/boot orchestration remains open.

## Actual verification

From the OrcSDR worktree root:

```powershell
cmake --build .local/maps/catalog-security-20261019 --config Debug --target map_delivery_state_test map_delivery_nvs_test
ctest --test-dir .local/maps/catalog-security-20261019 -C Debug -R '^map_(delivery_state|delivery_nvs|transfer)$' --output-on-failure
cmake --build .local/maps/catalog-security-20261019 --config Debug --target map_catalog_security_test
python apps/orcsdr-tab5/tests/maps/catalog_security/run_cases.py --exe .local/maps/catalog-security-20261019/Debug/map_catalog_security_test.exe --openssl 'C:/Program Files/Git/usr/bin/openssl.exe' --manifest F:/AI/OrcSDR-TEMP/orcmaps-quality-delivery/.local/global-assessment/global-z8-candidate.manifest.json
python .local/maps/check_map_transfer_syntax_20261019.py map_delivery_state.cpp map_delivery_nvs.cpp map_catalog.cpp map_catalog_session.cpp
```

- CTest **3/3 PASS**: state roundtrip, rollback/equivocation, corrupt/truncated
  records, failed commit/readback, bounded ownership, pending intent/completion,
  NVS adapter errors/handle closure, and existing transfer/activation cases.
- **37 signed cases PASS** through both the direct real consumer and persistent
  acceptance path. Includes product validation/duplicates, two valid distinct
  artifacts/products, signature/JSON/time/identity/URL limits, no output on
  rejection, saved trust, failed first commit, and same-payload flash-write skip.
- P4 syntax **4/4 PASS** with actual SDK headers/configuration. Sources are in
  native CMake, but no native firmware link/boot is claimed for this milestone.
- From the OrcMaps worktree root, `python tests/test_map_catalog_builder.py`:
  **11/11 PASS**, including product IDs and temporary P-256 signing/tampering.
  Temporary private keys are deleted; no production signing key was created.

The first missing-implementation probes were red. An initial host harness also
hit a vector assertion before its fixture existed; a precondition check fixed
that harness error before behavioral verification. This was not a device crash.
An independent scoped review caught optional rather than mandatory completion
intent. The added regression failed for the intended reason, then passed after
empty intent was rejected. Follow-up review confirmed the fix and found no
remaining verified blockers in the scoped source. This is not hardware acceptance.

Logs in the respective invoking worktree roots:
`map-delivery-state-{red,build-red,cases-red,build,cases,regressions,p4-syntax}-20261019.log`,
`map-delivery-intent-red-20261019.log`, `map-catalog-session-build-20261019.log`,
`map-delivery-required-intent-red-20261019.log`,
and `map-catalog-products-{red,test}-20261019.log` (last pair in both roots).

The NVS host test executes the production adapter against an explicit in-memory
SDK stub. It proves error/control flow, not ESP-IDF flash behavior or reboot
persistence. Shared transaction tests use simulated filesystem faults. No new
global archive hash/download/copy, network request, release or SD modification
occurred. Existing physical radio, SD and rendering acceptance does not extend
to these unused delivery APIs.

Next: connect trusted catalog fetch, pending intent, staged transfer, activation,
state commit/finalization and interruption recovery to the serialized map worker
and self-contained wizard; then native-build and verify on COM17. Final dashboard
integration, label controls and worldwide quality/performance gates remain open.

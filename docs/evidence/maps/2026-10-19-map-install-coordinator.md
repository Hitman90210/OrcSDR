# Coordinated install and worker recovery — October 19, 2026

Scope: source, focused host tests and P4 syntax. No native link, device access,
flash, network transfer or physical SD/NVS acceptance in this milestone.

## Transaction and existing worker

`install::run` now joins the existing operations in order:

1. Require a catalog matching persisted accepted revision/hash and currently
   valid trusted UTC. Caller supplies the immutable authenticated catalog.
2. Finish any journal already owned by durable state before another update.
3. Download/resume into hidden staging; verify archive readback and PMTiles.
4. Persist explicit product/target/selection intent without replacing ownership.
5. Activate while retaining prior metadata and archive bytes.
6. Persist replacement ownership and clear intent, then finalize the journal.

`install::resume` finishes durable intent without HTTP or a new catalog fetch.
It reconstructs metadata from the hash-bound hidden stage/intent, verifies its
identity and archive, and reconciles uncertain state commits. An owned committed
journal is finalized even when a write reached storage but reported failure.
Missing prior metadata permits reinstall on a replacement card; SD errors are
not treated as absence. Changed managed metadata fails rather than being guessed.

The existing map worker calls recovery before inventory scanning, using a 16 KiB
PSRAM buffer only when pending verification needs it. It does not hold the render
mutex during recovery. Per-chunk cancellation checks yield periodically; UI work
remains on the UI task. Local Maps shows recovery status, and Rescan retries a
failed/paused recovery. `ORC_MAP_INSTALL` diagnostics expose phase/busy/pending
without logging private endpoints. Normal map/receiver selection remains owned
by the existing UI callbacks; recovered maps are offered for explicit selection.

## Reusing an earlier artifact

A focused A → B → A regression reproduced a stale completion marker: an earlier
artifact's finalized directory was reused with a different previous manifest,
so its old `activation.done` could not represent the new transaction.

Before creating a new journal or changing visible maps, `prepare_reuse` validates
the finalized marker, preserves obsolete `previous.bak` metadata under
`/orcmaps/.recovery/<full-manifest-sha256>.bak`, then clears the stale marker.
Existing backup destinations must contain identical bytes; conflicts fail
without overwriting them. Prior archives are retained. Interrupted preparation
is retryable. The hidden history avoids extending already-long stage filenames
and is outside flat PMTiles manifest discovery. No automatic history cleanup is
implemented or authorized.

## Actual verification

From the OrcSDR worktree root:

```powershell
cmake --build .local/maps/catalog-security-20261019 --config Debug --target map_transfer_test
ctest --test-dir .local/maps/catalog-security-20261019 -C Debug -R '^map_(transfer|delivery_state|delivery_nvs)$' --output-on-failure
python .local/maps/check_map_transfer_syntax_20261019.py map_activation.cpp map_install.cpp map_downloads.cpp map_service.cpp map_screen.cpp
git diff --check
```

- Focused CTest **3/3 PASS**. Coordinated cases cover complete installs,
  failed/uncertain intent and ownership commits, offline retry without HTTP,
  four first-install interruption boundaries, five replacement boundaries,
  repeated A/B artifact reuse through catalog revision 6, preserved metadata,
  conflicting backup history, and untrusted/expired catalog rejection before
  network activity. Existing transfer/activation/state/NVS-stub cases also pass.
- P4 syntax **5/5 PASS** using actual SDK 5.5.4 flags/headers. Native CMake
  registers the new sources; this does not establish linking or running them.
- Read-only scoped review reproduced the artifact-reuse blocker. The new test
  failed for that reason before the correction; follow-up review confirmed the
  fix and identified no remaining verified blocker in its narrow scope.

Logs in the invocation directory:
`map-install-{red,build-red,cases-red,build,cases,regressions,p4-syntax}-20261019.log`
and `map-install-reuse-{build-red,red}-20261019.log`.

The C++ fixture is a real small gzip PMTiles archive padded to about 2 MiB.
Network, filesystem mutation boundaries and NVS commits are simulated. The
coordinator receives a constructed trusted catalog in transaction tests; actual
signature/persistent-acceptance behavior is covered separately by the 37 signed
consumer cases. No production trust key or catalog endpoint is selected.

## Remaining gates

Catalog HTTPS fetch, configured public trust, download list/progress/retry/cancel
controls in the self-contained wizard, and ordinary install commands on the
worker remain open. Only restart/retry recovery is currently wired into the
worker; the full network download UI is not enabled.

Physical rename/fsync/NVS durability, memory/stack margins, retained-map SD and
radio behavior require COM17 validation after the download flow is connected.
Global archive installation, worldwide rendering acceptance, dashboard migration,
label controls and the 33 ms performance goal are not closed by these tests.

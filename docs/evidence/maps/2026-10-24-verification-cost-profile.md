# Verification cost profile — prepared source

Follow-up: [COM17 native candidate and 20-pan result](2026-10-24-offscreen-native-refill.md). Build/flash/continuity pass; p95 refill fails. Earlier prepared-source statements below are historical checkpoints. Full-file verification profiling is installed but still unexercised.

## Scout finding

Worktree: `maps-integration-completion`, branch
`codex/maps-integration-completion`, base `origin/main`. Starting HEAD was
`a40da7ee`, with no tracked changes. Main reconciliation through `fb70e969` was
already recorded; remote state was not refreshed for this narrow local change.
No PR or review-comment triage is part of this change. The diff touches only
verification observation, its worker binding, focused tests and documentation;
it contains no inherited mainline fix.

Risk: **MEDIUM**, because instrumentation runs in the real verification path.
The next useful check is native timing under a bounded, explicitly recorded
load. Routine targeted build/COM17 authority is already recorded; this change
does not authorize publication or another full download.

## Why measure before changing behavior

The fresh-install call path performs a full hash/PMTiles check in
`map_transfer.cpp::prepare`, then checks the staged archive again in
`map_activation.cpp::apply` before publishing it. Recovery and existing-owned
paths have their own checks. The two fresh-install checks protect different
transaction boundaries; neither was removed or bypassed.

The October 24 PNY-card baseline measures 21m21.2s for the combined verification
and activation interval, under mixed load. It does not distinguish SD time,
SHA time, cooperative yields, preemption, or activation metadata. The source
alone cannot attribute that duration to any one of them.

## Prepared instrumentation

An optional `Hooks::verify_profile` receives one `VerificationProfile` on each
normally returned file-check outcome, including cancellation or rejection.
Allocation exceptions retain existing handling and do not promise a profile.
The worker writes a single serial line per observed pass:

```text
ORCMAPS_VERIFY_PROFILE bytes=... blocks=... read_us=... hash_us=... elapsed_us=... result=...
```

- `bytes`/`blocks`: successfully read and hashed data, including a cancelled
  prefix. They do not establish a valid archive.
- `read_us`: accumulated sequential block-read calls.
- `hash_us`: accumulated SHA-256 block-update calls.
- `elapsed_us`: whole check before automatic resource cleanup, including
  initialization/finalization, PMTiles validation, progress callbacks and yields.
- `result`: existing transfer error enum; 0=success, 6=cancelled, 8=hash failure.

All durations use the monotonic steady clock and include scheduling/preemption.
Read plus SHA time is not isolated CPU work; remaining elapsed time is not a
measurement of any single task. No path or coordinates are logged. No archive,
NVS, UI layout, dependency pin, or hash/activation rule is changed. Profiling
uses local scalar state, with no new image buffer or permanent PSRAM allocation.
Native stack/link cost and measurement overhead remain unmeasured.

## Tests actually run

```text
cmake --build .local/maps/catalog-security-20261019 --config Debug --target map_transfer_test
ctest --test-dir .local/maps/catalog-security-20261019 -C Debug -R ^map_transfer$ --output-on-failure
python .local/maps/check_verify_profile_syntax_20261024.py map_transfer.cpp map_downloads.cpp map_install.cpp map_activation.cpp
```

**Transfer regression 1/1 passed (5.47s)**. Added assertions cover one successful
profile with exact bytes/block count, a cancelled prefix with unchanged archive,
and a complete scan that still rejects corruption. Timing assertions check only
accounting consistency, not desktop speed. Existing resume, transaction and
fault-injection coverage in that test remains enabled.

**Four P4 syntax checks passed**. No full suite, native build, flash, SD mutation,
or large-file scan was performed for this source change. COM17 remains on
physically accepted `c552111f`; COM39 untouched.

Logs saved beside invocation:
`maps-verify-profile-host-build-20261024.log`,
`maps-verify-profile-host-test-20261024.log`,
`maps-verify-profile-p4-syntax-20261024.log`.

Next milestone: combine native profiling with the next necessary verification
run, record per-pass bytes/read/SHA/elapsed and workload, then choose a correction
from those measurements. Do not claim that the long verification is resolved.

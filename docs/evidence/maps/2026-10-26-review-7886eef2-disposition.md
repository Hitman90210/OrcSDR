# PR #211 bounded review disposition

CodeRabbit review 5648912293 examined 89 selected files at `7886eef2`, including
map/lifecycle/storage tests, shipping map/setup documentation and release notes.
The earlier unrestricted review was rejected at 352 files. This pass is actual
review coverage of those 89 files, not a claim that all historical evidence was
reviewed. Restore unrestricted filters after the follow-up pass.

| Finding | Classification and disposition |
| --- | --- |
| 4375026074, recording display color overload | FIX BEFORE MERGE: a fixed five-argument color template now records uint16_t clears and removes overwritten text. A regression exercises that exact type; all 16 configured map CTests pass. |
| 4375026083, ADS-B stale sidebar range | FIX BEFORE MERGE: zoom updates the bounded range value as well as the map, even without a live snapshot. No full-screen repaint or range-limit mutation. Focused handler regression passes. |
| 4375026099, cross-task setup cleanup | FIX BEFORE MERGE: remove UI-owned cleanup from the shared receiver queue routine; perform it after the documentation transition and before UI-loop early returns. The receiver queue still defers starts during setup. Lifecycle regression passes. |
| 4375026115, historical consumer matrix status | FIX BEFORE MERGE: label the October 17 checkpoint as historical and link the current release ledger. |
| 4375026121, purported missing relative links | STALE/IGNORE: all 19 relative targets in the two cited documents exist in the worktree and in reviewed commit `7886eef2`, verified with `git cat-file -e 7886eef2:<target>`. Do not replace working evidence links based on a failed reviewer lookup. |
| 4375026127, user-guide pilot claim | FIX BEFORE MERGE: describe the activated signed production catalog with no scheduled expiry. |
| 4375026134, inconsistent offline-map guide status | FIX BEFORE MERGE: distinguish the production catalog from firmware release-candidate acceptance; remove obsolete pilot wording from the archive checksum paragraph. |
| 4375026141, LoRa test parent directory | FIX BEFORE MERGE: create harness parents in a fresh checkout. |
| 4375026154, catalog clock test parent directory | FIX BEFORE MERGE: create harness parents in a fresh checkout. |
| 4375026165, SSTV test parent directory | FIX BEFORE MERGE: create harness parents in a fresh checkout. |
| Trivial HTTP transport stub finding | FIX BEFORE MERGE: mirror IDF's UNKNOWN/TCP/SSL enum. A regression first failed because an unset transport equalled SSL; it now passes with the production adapter's explicit SSL setting. |

Grok's closed-packet review of `16ab1610..4ca5e23d` found one BLOCKER: the new
download worker used a PSRAM stack while its install/catalog paths can commit
NVS. The installed IDF cache-disable guard requires an internal/RTC stack.
The corrected design allocates an internal 8 KiB WithCaps stack only for a queued
job, suspends after completion, and reaps it on the UI task. Idle maps therefore
do not permanently consume another internal stack or reduce pairing headroom.
Allocation failure is visible and preserves retry identity. The render worker
continues independently, with discovery/recovery serialized against activation.
No failing production catalog write was deliberately attempted on the unsafe
candidate. The internal-stack path still needs device validation.

## Verification completed

- `bash tools/test-tab5-cadence-history.sh`: focused cadence, history, lifecycle,
  zoom, download scheduling/startup/reaping, writeback and DSP-stage tests pass.
- Rebuilt configured map host targets; CTest 16/16 passes.
- Exact P4 compile-command syntax checks pass for main, map service/downloads
  and ADS-B.
- Raw logs beside invocation: `pr211-review-on-demand-host-20261026.log`,
  `pr211-review-on-demand-map-{build,tests}-20261026.log`,
  `pr211-review-on-demand-p4-syntax-20261026.log`.

These are source/host/syntax results. The corrected source must be linked and
tested on-device before closing the hardware gate. IQ continuity remains open.

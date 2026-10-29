# PR #211: mainline reconciliation and review gate

User approved preserving the eight newer main commits in the Maps branch,
reviewing/fixing valid major and minor findings, then merging and tagging after
validation. Hosyond work is explicitly excluded. Release version is awaiting the
user's selection. Existing dragging/refill performance is accepted for release;
the old p95 <=2s target is deferred, not claimed passed.

## Source

Clean starting HEAD `9e747467`; current main `07c777ca` is eight commits ahead
of the shared ancestor. Those are the seven Shortwave/Home changes in PR #210
plus its merge. Incoming diff: 13 files, 683 additions / 40 deletions. No Hosyond.

`git merge-tree --write-tree HEAD origin/main` simulated without conflicts;
`git merge --no-ff --no-stat origin/main` created **bcb618e8**. Both histories are
retained, with no rebase/force-push. **b0542203** additionally corrects the two
verified documentation failures and starts a temporary CodeRabbit source pass.
The branch is pushed to existing draft PR #211; main itself is not merged here.

CodeRabbit skipped the original oversized diff. The new configured source pass
contains 96 changed files plus its config, under the account's 100-file limit.
The bot confirms full review triggered and check reports in progress. At the
bounded follow-up snapshots it has zero formal reviews / inline findings.
That is pending review, not clean review. Planned second pass covers 73 tests,
build files and shipping docs; restore broad config before merge. Historical
evidence files are not represented as externally reviewed.

## Tests actually run

- Shortwave SSB and model host executables built/run with WSL g++: pass.
  Map pipeline CTest: 1/1 pass. Command retained in
  `maps-pr211-shortwave-host-20261025.log`.
- `python3 apps/orcsdr-tab5/tests/navigation_graphics_tests.py` under WSL: pass.
  First Windows invocation failed because g++ is unavailable there; no firmware
  fix was needed. Both logs preserved as `maps-pr211-navigation{,-corrected}-20261025.log`.
- `python tools/check_documentation_truth.py`: 0 errors/warnings after adding
  Maps to architecture IDs and refreshing measured main.cpp size.
- `python -m mkdocs build --strict`: pass after changing the guide's out-of-tree
  evidence reference to a GitHub link. GitHub's corresponding checks now pass.
- P4 syntax: main.cpp, home_dashboard.cpp, shortwave_dashboard.cpp,
  shortwave_ssb.cpp, map_screen.cpp all pass. Exact invocations retained in
  `.local/maps/check_pr211_merge_syntax_20261025.py`.
- Native build/inspection of b0542203 pass. App 5,295,232 bytes; headroom
  996,224 bytes. DMA reserve 40 KiB, exception reserve 1 KiB, dependency/symbol
  and PSRAM placement gates pass. App SHA-256
  `94fbc5da8ac46a40c83195ad0ff966401d7ca5e035e235b3bd0c7e2312f42643`;
  ELF `e8c25ecd33eb9bf39c80a2e00b9b0f06e06b811661996af46a20850622a76416`.

No broad local suite or new decoder acceptance claim. CI checks currently pass
other than pending external review; skipped docs deployment is expected for a PR.

## COM17 evidence — functional checks pass, continuity concern open

Identity **30:ED:A0:E2:E6:95**. Preflight Home/FM 94.5 MHz at approximately
2.4 MSPS, zero reported drops/overruns. Previous production app verified before
write; fresh NVS/PHY/coredump/world backups and app/non-app flash verification
pass. P4 app-only update; no SD pack/world/C6 update or COM39 access.
Artifact/recovery: `.local/maps/maps-pr211-shortwave-b0542203-20261025/`.

Native monitor matches ELF and reaches SD/Wi-Fi/RTL Ready. Authenticated plan
`.local/maps/pr211-native-regression-plan-20261025.json` completes: production
catalog Check, Setup Next/Skip/Back/Cancel, map picker open/pan/cancel, saved
location unchanged, return Home graphics=1, FM 94.5 MHz WBFM restored. Largest
DMA block 20,480 bytes; PSRAM free 17,294,764 / largest 16,777,216 bytes.

However, final driver counters report **51 drops/overruns**. The snapshot during
paused setup was zero. The follow-up localization plan proves Home idle counters
remain 51, picker reopening stays 51, a picker pan raises them to **60**, and
returning Home/idle remains 60. Thus the observed extra nine losses occur during
full-screen map work at this FM rate. This is not attributed to the newer
Shortwave code without comparison evidence. The driver, DSP, transport settings
and managed sources have not been modified. No speculative mitigation applied.
The earlier 960 ksps LoRa checks do not establish loss-free 2.4 MSPS operation.

Logs beside invocation: `maps-pr211-native-{build,inspection}-20261025.log`,
`maps-pr211-flash-console-20261025.log` and per-part logs,
`maps-pr211-boot-native-monitor-20261025.log`,
`maps-pr211-native-regression-device-20261025.log`,
`maps-pr211-fm-drop-localization-device-20261025.log`.
Host/docs/syntax/review snapshots use the same `maps-pr211-` prefix.

## Before merge/tag

Complete external review passes and fix verified findings; restore broad review
configuration. Investigate the 2.4 MSPS map-associated IQ loss, rather than claiming
all radio continuity tests pass. Record release version, final integrated checks
and remaining packaging/first-boot gates. User accepted map responsiveness, not
an unqualified loss-free radio claim. No merge to main or tag has been created.

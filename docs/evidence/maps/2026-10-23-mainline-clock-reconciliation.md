# Mainline clock follow-ups reconciled locally

User-approved scope: reconcile the nine incoming main commits through
`fb70e969b88628987829a582d6d2edef02c283ee`; inspect maps overlap and run
affected clock/UI/map checks. Decoder testing and audio-fixture inspection
were excluded.

## Result

Local merge commit: `97f486408b1edc1aa51ed71fbd722f955787b7e9`.
Parents: maps checkpoint `d3887347` and fetched main `fb70e969`.
The merge was conflict-free. `git merge-base --is-ancestor fb70e969 HEAD`
returned 0; `git rev-list --count HEAD..origin/main` returned 0 immediately
after the merge. This establishes the fetched baseline, not future main changes.

Reviewed overlap: main component registration, FT8 dashboard/snapshot/actions,
runtime clock hooks, shared UI command/service entry points and their tests.
The incoming line adds WWV measurement and persistent clock trust, plus the
RTL AGC popup control. No map or setup implementation files changed in this
merge. Existing map controls, setup ownership/pause and graphics restoration
remain in the combined source. No integration-specific correction was needed.

## Commands and actual results

From `F:/AI/OrcSDR-TEMP/maps-integration-completion`:

```text
wsl bash /mnt/f/AI/OrcSDR-TEMP/maps-integration-completion/.local/maps/reconcile_fb70e969_20261023.sh
```

Exit 0. Three individually compiled host programs passed:
`clock_sync_status_tests`, `ft8_dashboard_tests` and
`ft8_world_projection_tests` (including the full 824x420 pixel-centre sweep,
seams and poles). The script builds only four selected CMake targets, then runs:

```text
ctest --test-dir .local/maps/feature-replay-integration-20261023 -R '^(map_pipeline|map_feature_cache|setup_ui|setup_flow)$' --output-on-failure
```

All four selected tests passed. This was not the full suite or a new worldwide
archive comparison. The configured map harness uses the local OrcMaps checkout;
this does not validate a new published engine pin.

```text
python .local/maps/check_reconciliation_syntax_20261023.py main.cpp ft8_dashboard.cpp ft8_runtime.cpp clock_sync_status.cpp ntp_sync.cpp wwv_tick.cpp
git diff --cached --check
```

All six P4 syntax compilations exited 0 using saved native compile flags.
Whitespace check passed before committing. Syntax checks do not prove linking,
firmware size or hardware behavior.

Saved logs in the test working directory:

- `maps-mainline-reconciliation-merge-20261023.log`
- `maps-mainline-reconciliation-host-20261023.log`
- `maps-mainline-reconciliation-p4-syntax-20261023.log`

## Hardware and remaining gates

No hardware used, no flash, no reset, and no SD/NVS/C6 changes. COM17's installed
app remains `f38b8601`; COM39 was untouched. No push, main merge, release or
shared-branch rebase occurred. Untracked logs and recovery artifacts remain.

The next performance milestone remains a verified shared-engine candidate and
a bounded COM17 measurement against p95 detail refill <=2 seconds. The latest
installed result (3.55 seconds p95) still fails that gate. Producer regression/
publication approval and CodeRabbit triage remain separate pending decisions.
Native build/link, preservation checks and hardware acceptance of the combined
source are not closed by this reconciliation.

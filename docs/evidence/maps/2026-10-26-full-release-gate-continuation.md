# Full release gate continuation — October 26, 2026

## Superseding owner release decision

After the conducted SSTV checks, the owner instructed "merge our work" and
"im happy enough with this to push for the release now. no further release gate."
This explicitly supersedes the earlier requirement to finish every required row
before merge/tag. Preserve all FAIL/BLOCKED/untested evidence and the unresolved
watchdog observation; none is reclassified PASS. Proceed with PR #211 merge and
v0.5.0-beta.1, with build/package integrity checks and honest beta limitations.
Source merge/tag, firmware asset availability and M5Burner publication remain
separate observable actions.

The owner explicitly retained the full decoder-by-dongle gate after the logger
repair. No merge, tag or publication is authorized by a partial test pass.
The accepted transient map-interaction IQ loss and three V5 recovery cycles
remain scoped exceptions; they do not waive other required tests.

## Current checkpoint

- Branch: `codex/maps-integration-completion`, head `ef5cd33f`.
- Tested COM17 app source: `fd8b6de6`; later commit is documentation only.
- App SHA-256: `3c700d175e9d5f35f507bbb3d8c836c870b2fe61f6623df8d6ccf3987350b5e1`.
- PR #211 remains draft/open. Current CI passes. CodeRabbit's summary records
  `fd8b6de6` as reviewed with no actionable comments; `ef5cd33f` documentation
  was not included in that incremental review.
- Fetch of origin/main completed; head is 239 commits ahead, zero behind.
- COM17 identity `30:ED:A0:E2:E6:95`: Nooelec V5/profile 3, driver 0.9.3,
  streaming 906.875 MHz at approximately 960,000 samples/s, gain AUTO, bias off.
- Read-only sample: uptime 3,796,097 ms, reset reason 11, driver drops/overruns
  zero, logger ready, queued/pending/dropped zero, retry count one.
- Memory sample: PSRAM free 3,846,104 bytes; internal free 26,275 bytes;
  largest DMA block 14,848 bytes. These are observations, not a memory gate pass.

## Tests executed at this checkpoint

| Command | Actual result | Evidence beside invocation |
|---|---|---|
| `powershell -NoProfile -ExecutionPolicy Bypass -File apps/orcsdr-tab5/tools/run-release-readiness.ps1 -SelfCheck` | PASS, `ORC_RELEASE_SELF_CHECK pass=1`; report-formatter self-check only | Console |
| `bash tools/test-pocsag-core.sh` | PASS, optimized and ASan/UBSan cases | `release-gate-packet-host-20261026.log` |
| `bash tools/test-pocsag-store.sh` | PASS, normal and sanitizer cases | Same log |
| `bash tools/test-acars-smoke.sh` | PASS, smoke/CRC/FEC/sync | Same log |
| Existing authenticated COM17 helper, read-only status plan | PASS, no reset or tuning requested | `release-gate-readonly-com17-20261026.log` |

Host checks do not qualify any dongle RF path. The existing readiness runner's
PASS report does not aggregate the full four-dongle matrix; treating it as the
complete release decision would be incorrect.

## Required evidence still to close

| Area | Status | Reason / next step |
|---|---|---|
| LoRa controlled reception and persistence | BLOCKED for final matrix completion | Baselines exist for all four dongles; repair app validated on V5. Reconcile per-firmware applicability and complete required repetition/load/source-off evidence. |
| FM/RDS and AM/USB/LSB on four dongles | BLOCKED | Need known source, measured chain, audio/sideband/manual checkpoints and separate HF-path evidence. |
| ADS-B, P25, ACARS, POCSAG live rows | BLOCKED | Need validated protocol fixtures/generators, exact expected output and three successful controlled repetitions plus source-off controls. Host synthetic modulation alone is insufficient. |
| Enabled digital modes live rows | BLOCKED | Match actual shipping modes, timing/reference messages and each applicable dongle; do not infer from CI or another branch's tests. |
| SSTV conducted Robot36/PD120/Scottie S1 | BLOCKED | Reusable source harness exists; current cable chain and per-band level not confirmed. |
| Watchdog observation | BLOCKED | Previous reset remains unexplained; stale coredump cannot establish its cause. No new reset in this bounded read-only sample. |
| Final tagged package/M5Burner validation | BLOCKED | Depends on qualified firmware, merge verification and exact tagged package. |

BLOCKED here means required evidence is not yet complete, not a newly established
decoder defect. Existing detailed logs remain the source of individual results.

## Conducted-source readiness

Read-only IIO discovery found Pluto at `192.168.1.228`, serial
`b0d85d89da56de55b2ee997b00499360`; historical addresses were not reused.
`python -m orc_sstv check --uri ip:192.168.1.228` passed identity/firmware/counter
preflight: FISH Ball Rev.A / AD9361, v2.3, readable DMA counters, no reported
problems. Both TX attenuation attributes read -89.75 dB. This read-only operation
did not start TX and does not prove current source ownership, TX mute or measured
receiver input power.

Source log: `F:/Ai/PlutoSDR/sstv/orc-sstv-20261026-160615-567649.log`.
The owner was asked to prepare TX1 → 30 dB pad → cable and tinySA Ultra for fresh
level measurement. Live RF remains pending that execution checkpoint. Keep TX
off during connection changes and invalidate attempts with source DMA faults.
Use comfortable measured levels; the historical weak-signal calibration is not
current qualification evidence.

Next milestone: establish the measured conducted chain, then run the same
validated protocol sequence on each dongle, preserving explicit manual checks.

## Conducted SSTV follow-up

The chain was subsequently corrected, measured and connected to COM17's V5.
Robot36, PD120 and Scottie S1 each completed and saved images that passed the
existing reference-image limits at 40.2 dB receiver gain. Earlier 20.7 dB
acquisition failures remain recorded. The supported-mode host regression also
passed. Full settings, source hashes, image metrics, logs and remaining physical
checks are in the [V5 conducted record](2026-10-26-v5-sstv-conducted.md).
This closes these three V5 mode/image/storage checks, not the full matrix row
across all dongles. COM17 was left IDLE and a Blog V4 swap requested with TX muted.

The owner subsequently connected Blog V4 on COM17 through the same cable.
All three conducted SSTV modes also completed, saved and passed reference-image
comparison on V4; see the [V4 conducted record](2026-10-26-v4-sstv-conducted.md).
COM17 was again left IDLE with TX muted, and its five recovery cycles were
requested as the next manual checkpoint. V3c/V4L conducted SSTV and the remaining
decoder rows remain open. The owner's later report that the test images looked
good establishes observed image appearance, not unobserved audio/control checks.

An early V4 unplug/replug overlapped serial tuning preparation and operator
navigation to ADS-B. A later query confirmed an unintended uptime restart with
reset reason 7; no new crash dump was retained. The five-cycle attempt was not
scored. This [new reset observation](2026-10-26-v4-hotplug-navigation-reset.md)
remains a release blocker; an isolated recovery retry is not a substitute for
investigating the mixed sequence.

The operator reported three successful V4 recovery cycles. The timed automated
monitor recorded none before it expired, so the results are distinguished:
three operator-reported recoveries, zero automatically verified cycles, original
five-cycle gate incomplete. A subsequent end-state readback was healthy, and
three navigation-only SSTV/ADS-B passes completed without a restart. This does
not close the mixed disconnect/navigation reset investigation. Details and logs
are in the same incident record.

One subsequent controlled V4 disconnect with serial navigation to ADS-B also
recovered without a reset; sustained stream, safe attach defaults and saved-data
persistence were verified. A raw recorder expectation incorrectly required
manual gain retention, so its INCOMPLETE result is retained alongside a separate
source-verified recovery analysis. Neither assessment closes the earlier reset
or original five-cycle gate. COM17 was stopped and Blog V3c requested for the
next conducted SSTV block.

V3c subsequently completed Robot36, PD120 and Scottie S1 with saved-image
comparisons passing. Two Pluto DMA-underflow attempts were invalidated and
retained as source failures, not decoder failures; clean full retries supplied
the scored images. The explicit memory-reclaim command released retained LoRa,
map and SSTV buffers. See the [V3c conducted record](2026-10-26-v3c-sstv-conducted.md).
V4L conducted SSTV is the remaining dongle block for these modes; the other
protocol, physical and reset gates are still open.

V4L subsequently completed the same three conducted SSTV modes on COM17, with
successful SD readbacks and reference-image comparisons. Thus the mode/image/
storage subset is now PASS for all four dongles (12 complete frames); invalid
source attempts remain excluded and documented. See the
[V4L record](2026-10-26-v4l-sstv-conducted.md). The complete SSTV/application-load/
physical gate and other protocol rows are not automatically closed. COM17 was
left Home/IDLE with the V4L attached through the attenuated cable, Pluto muted,
and COM39 firmware unchanged.

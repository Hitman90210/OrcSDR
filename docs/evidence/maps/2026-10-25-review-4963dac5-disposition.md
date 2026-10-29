# Review of pushed head 4963dac5: verified findings and disposition

CodeRabbit review 5648010017 completed against `4963dac5`. It posted two inline
findings and one outside-diff LoRa finding. Grok's closed-packet latest-15 review
completed with exit 0 and one MINOR finding. Both reviews were evaluated against
current code and explicit user decisions; neither is a whole-PR clearance.

| Finding | Classification | Disposition |
| --- | --- | --- |
| Downloader BACK should return to inventory | STALE/IGNORE: requirement mismatch | The user explicitly wants to leave maps and continue using the device without cancelling the transfer. BACK exits the map view; INSTALLED remains the separate inventory action. Existing host regression asserts exit without cancelling. No behavior reversal. |
| Restore full archive SHA-256 before promotion | STALE/IGNORE: superseded requirement | The user explicitly removed full-file scans in favor of a quick size/metadata check. The limitation is real and documented: same-size payload corruption beyond metadata is not detected. Catalog signatures/HTTPS/transaction protections remain; no hash-verified claim. Do not restore scans against that instruction. |
| Rejected LoRa auto capture can latch ready forever | FIX BEFORE MERGE | Reproduced with the production handoff routine. If decoder startup resources are unavailable, release only the automatic completed slot, reset its flags/write count, and count/log a drop. Preserve manual captures and the busy-decoder pending handoff; queue-send failure is counted once. |
| Grok: FT8 retains unusable partial sprite pair | FIX BEFORE MERGE | Reproduced world-sprite-only failure with tracked test allocations. Free the unusable partial pair before the bounded retry. Completed map pairs remain preserved. |
| Earlier CodeRabbit minor: SSTV reports unknown free space as full | FIX BEFORE MERGE | Expose bool `storage::query_free_bytes`, retaining existing free_bytes behavior for other consumers. SSTV maps query failure to its UINT64_MAX advisory sentinel, attempts the real write and reports its actual result; known zero remains no_space. |

Grok's wording called the world sprite larger; actual sizes are 524,288 and
692,160 bytes. The reproducible issue is partial retention, not that size claim.
Raw Grok verdict:

> MINOR apps/orcsdr-tab5/ui/ft8_dashboard.cpp:787 - map canvas stays allocated when world-canvas createSprite fails; the 2s retry calls reclaim_all and retries only the missing sprite, so the retained canvas can keep the larger world sprite from fitting until leave().

## Validation actually run

Run directory: `F:\AI\OrcSDR-TEMP\maps-integration-completion`. Logs outside Git:

- `python tests/lora_auto_handoff_test.py`: red reproduced latched ready flags;
  green PASS. Tests automatic unavailable/startup-recovery, manual preservation,
  busy decoder pending, missing queue/buffer, and queue-send failure counted once.
  `pr211-lora-handoff-{red,green}-20261025.log`.
- Focused FT8 recording-display executable: world-only allocation failure red;
  ASan/UBSan green PASS, including recovery and existing no-clear assertions.
  `pr211-ft8-partial-{red,sanitized}-20261025.log`.
- `python tests/sstv_space_query_test.py`: red reproduced unknown-as-zero;
  green PASS, `pr211-sstv-space-{red,green2}-20261025.log`.
- `python tests/sd_space_query_test.py`: PASS, including bool query, null output,
  known zero and query failure; `pr211-storage-query-green-20261025.log`.
- Real SSTV archive suite with ASan/UBSan: PASS, including unknown-space write,
  known full card and existing storage-fault/slow-writer checks;
  `pr211-sstv-archive-sanitized-20261025.log`.
- `python .local/maps/check_pr211_review_fixes_final_20261025.py main.cpp ft8_dashboard.cpp sstv_archive.cpp orcsdr_storage.cpp`:
  all four P4 syntax exits 0; `pr211-review-fixes-p4-syntax-final-20261025.log`.
  An earlier syntax run caught that the FAT helper was private; corrected by
  the explicit checked public query API before this final passing check.
- `git diff --check`: PASS.

Grok packet `.local/maps/grok-pr211-latest15-packet-20261025.md`; raw output
`grok-pr211-latest15-review-file-20261025.log`. Initial launch exceeded Windows
argument length; switching to the CLI's prompt-file input succeeded. MCP startup
warnings did not prevent a final parseable verdict. Shell, writes, repository
reads, web and subagents were denied for this closed-packet review.

These source corrections have not been natively linked or flashed at this
checkpoint. Both tablets still run the previously validated `9e3f0378` app.
Review fixes do not close the other recorded PR/release-matrix or IQ-loss gates.

# Reconciled candidate: 20-pan detail refill

COM17 USB serial `30:ED:A0:E2:E6:95`; installed source `137e244a`, published
OrcMaps pin `2133547`. Test spans local midnight October 23–24. Existing filenames
retain their start-date suffix. COM39 untouched; no flash in this measurement.

## Actual workload and result

From `F:/AI/OrcSDR-TEMP/maps-integration-completion`:

```text
python .local/maps/com17_label_bench.py maps-reconciled-refill-plan-20261023.json maps-reconciled-refill-device-20261023.log
python .local/maps/summarize_reconciled_refill_20261023.py
```

Both exit 0; 20 distinct completed pan generations verified. Pattern:
`(40,0), (0,20), (-40,0), (0,-20)` repeated five times. Existing LoRa map view,
receiver-centered, 842x330, labels off; six tiles / 5,763 features per sample.
Seven-second collection after each pan, then named status. Nearest-rank p50/p95;
these are worker wall times including yields and reception scheduling, not
end-to-end gesture latency or isolated drawing CPU benchmarks.

| Worker stage | p50 | p95 | Maximum |
|---|---:|---:|---:|
| Total detail refill | 2.865801 s | 2.890615 s | 2.895334 s |
| Replay/stream | 0.854976 s | 0.905306 s | 0.908699 s |
| Render/flush | 1.933800 s | 1.991114 s | 2.026543 s |
| Tile I/O | 0 | 0 | 0 |

**The p95 <=2-second gate still fails.** Every sample has six replay hits,
702,110 retained bytes in the 768 KiB arena, zero missing/failed tiles and zero
backing reads. The earlier f38b8601 run had p95 3.549776 seconds; this new result
does not establish an isolated optimization multiplier. Source/mainline and
radio conditions differ. Driver reports V4/R828D, manual gain 280 initially and
338 at the end; this was not a fixed-gain RF comparison. Antenna/band suitability
was not revalidated, so no decoder/reception-quality acceptance is claimed.

Driver snapshots while streaming at 906.875 MHz: approximately 955–959 ksps,
zero drops/overruns. LoRa SD logger ready, error=false, queued=0, dropped=0.
Loaded PSRAM: 3,543,396 initially and 3,544,172 bytes finally; largest block
3,473,408. Largest internal DMA block 18,432 bytes; main stack high-water mark
1,564; reset reason unchanged and uptime advances. These are bounded snapshot
observations, not a soak or proof against every transient failure.

## End state and evidence

The harness stopped reception successfully and reopened setup. Final status:
setup active, Welcome step, saved location unchanged at E7 440628866,-1230160273.
No working map removed. The user-requested fresh GitHub install/resume/timing
test still awaits SD-card preparation; this pan benchmark is not that test.

Saved beside invocation:

- `maps-reconciled-refill-plan-20261023.json`
- `maps-reconciled-refill-device-20261023.log`
- `maps-reconciled-refill-console-20261023.log`
- `maps-reconciled-refill-summary-20261023.json`
- `maps-reconciled-refill-summary-20261023.log`

Next performance gate remains the prepared shared-engine offscreen rejection
candidate, subject to its pending full-regression/development-push approval,
then an inspected pinned native build and the same bounded workload. Physical
appearance and approximately 33 ms continuous cached motion remain separate
unclosed gates. No new source optimization is justified by calling this a pass.

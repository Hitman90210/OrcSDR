# COM17 routine-pan refill baseline — October 23, 2026

Application 30a00ce1; documentation checkpoint b6d882a5; engine pin
b59d47819cc3b81c1bbe50ed75b775275bb0218e. COM17 USB serial
30:ED:A0:E2:E6:95 verified. COM39 untouched. Controlled native-monitor reset
establishes fresh allocation state; no flash, erase or stored-location change.
Monitor terminal control was unavailable because its stdin pipe was closed;
only the exact matching owned monitor processes were terminated to release COM17.
This is a harness-control limitation, not a device fault.

## Workload and result

LoRa dashboard map, centered on saved receiver E7 440628866,-1230160273,
zoom 9 (global archive source zoom 8), labels off, 842x330 map. After a ready
warm-up, run (40,0), (0,20), (-40,0), (0,-20) pixel pans five times while
receiving at 906.875 MHz / 960 kS/s. Each pan has seven seconds before the
status sample. All 20 completed samples have distinct generations, six tiles,
5,763 features, missing=0 and failed=0. This is a warm routine-pan loop within
the same tile footprint, not a cold worldwide or boundary-crossing benchmark.

Percentiles use nearest rank, n=20. These are worker detail-frame wall times,
not gesture-to-pixel latency; transfer and cached-motion acceptance stay separate.

| Measurement | p50 | p95 | Maximum |
|---|---:|---:|---:|
| Detail frame | 3.651317 s | 4.552282 s | 5.134721 s |
| Stream/decode accounting | 2.520082 s | 3.292270 s | 3.442466 s |
| Drawing accounting | 1.139285 s | 1.494683 s | 1.657365 s |
| Read accounting | 0.394135 s | 0.537042 s | 0.588610 s |

**FAIL against the user-selected p95 <=2.0 s refill gate.** Read time and
cooperative waits overlap other accounting; do not add all rows or describe
subtraction as isolated CPU time. Active LoRa processing and adaptive gain vary;
this is a device workload comparison, not an isolated engine speed multiplier.

## Safety observations

No reset during the pan plan; main-stack high-water mark 1,448. After map warm-up,
free PSRAM moves from 3,664,836 to 1,697,984 bytes; largest final block 1,638,400;
largest DMA stays 19,456. This low headroom limits additional caches. It does not
by itself identify which growing holder owns the difference. Driver remains
STREAMING, drops=0/overruns=0 before/after pans, effective sample rate about
959 kS/s. SD logger ready=true,error=false,queued=0,dropped=0. Antenna/band-qualified
decoding is outside this run. FM 94.5 MHz/manual gain445 and Home restored;
final effective sample rate 2.33 MS/s, drops=0/overruns=0.

## Commands and logs

Run from F:/AI/OrcSDR-TEMP/maps-integration-completion:

```powershell
python .local/maps/com17_label_bench.py maps-refill-baseline-plan-20261023.json maps-refill-baseline-device-20261023.log
```

Exit0; companion maps-refill-baseline-console-20261023.log. Parsed rows and
percentiles: maps-refill-baseline-summary-20261023.json. Native startup capture:
maps-refill-baseline-startup-20261023.log. Logs reside beside invocation.

In the engine checkout, a bounded host probe reads sixteen nearby z8 tiles:
compressed 10,553–106,699 bytes, gzip ISIZE 14,285–158,240 bytes. Exact probe source
.local/global-assessment/probe_refill_sizes_20261023.cpp; compile/run result
maps-refill-tile-sizes-20261023.log, exit0. No full archive scan was performed.

## Next experiment

Prepare an optional single-tile inflate buffer capped at256KiB in OrcMaps.
Default stays two-pass streaming; oversized tiles and allocation pressure must
retain that path. Compare complete geometry/properties and rendered output,
then measure native benefit before accepting. No claim that it meets2seconds
or fixes cached-motion cadence follows from these baseline numbers.

Physical check received: user confirms dragging, colors and controls look good on the reconciled candidate. Detailed refill still fails the numeric gate above.

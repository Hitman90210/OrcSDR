# LoRa detailed-map refill baseline — October 22, 2026

Status: measured baseline only; no performance correction or firmware flash.
Source checkpoint: `2a8dfd8f`; previously verified installed app `51318792`.
COM17 USB serial `30:ED:A0:E2:E6:95`; COM39 untouched. No reset, SD write,
receiver-location save, pack change or radio retune was requested.

## Execution and actual result

From `F:\AI\OrcSDR-TEMP\maps-integration-completion`:

```powershell
python .local/maps/com17_label_bench.py lora-detail-refresh-scout-plan-20261022.json lora-detail-refresh-scout-device-20261022.log
python .local/maps/com17_label_bench.py lora-detail-refresh-baseline-plan-20261022.json lora-detail-refresh-baseline-device-20261022.log
```

Both exit 0 and record `PLAN COMPLETE`. Existing authenticated serial control
uses the existing local automation key; no pairing or credential changes.
The second command runs four `RTL_LORA_MAP PAN` actions, alternating `120 0`
and `-120 0`, waiting 12 seconds each, then querying map status. The dashboard
was already active. Zoom remains 9, stored source tiles are limited to zoom 8.

| Frame generation | Tiles/features | Detail frame (ms) | SD read time (ms) | Last completed-image push (ms) |
|---|---:|---:|---:|---:|
| 125 | 4 / 5444 | 6709.108 | 1154.226 | 26.835 |
| 126 | 6 / 5763 | 7245.289 | 1443.723 | 27.425 |
| 127 | 4 / 5444 | 6701.254 | 1144.986 | 28.077 |
| 128 | 6 / 5763 | 7661.164 | 1447.663 | 26.353 |

All frames report missing=0, failed=0. Four-tile frames read 358 times / 700178
bytes; six-tile frames read 400 times / 767298 bytes. Repeated views still rebuild.
IO time is included in streaming work; do not add these columns to total frame
time or treat streaming/render timings as pure CPU measurements. Scheduling and
cooperative waits can be included. Polling after completion is not a physical
interaction-latency measurement. These four samples do not establish a p95.

Final camera equals the starting camera: latitude E7 440173511, longitude E7
-1230054699, zoom 9. This is the map camera, not a receiver-location save.
Final radio status: STREAMING, 906875000 Hz, effective_sps=959767, drops=0,
overruns=0. Final health: PSRAM free=2089748, largest=1998848; internal free=16083,
largest DMA=8192; main stack high-water mark=1128, reset_reason=11 unchanged.

Logs are saved beside the test invocation in the worktree root. Console output
for the baseline is `lora-detail-refresh-baseline-console-20261022.log`.
Serial was closed after completion. No host suite or new native build ran.

## Next bounded investigation

Separate worker sleep/yield time from decode/draw time before changing scheduling.
Review compressed tile reuse (the stream currently reads/inflates each tile twice)
and avoiding rebuilding unchanged tile work. Preserve the last visible map,
cancellation and UI ownership; measure cache allocation/reclaim and active-radio
continuity. Do not assume SD alone explains the delay or promise a speedup.

# Single-inflate experiment rejected on COM17 — October 23, 2026

Engine experiment 7ed0d328ea127971817f81cdf28f131c64807f17; consumer 3fcd29e3.
COM17 identity 30:ED:A0:E2:E6:95 verified before each flash operation. COM39 was
not accessed. This is a failed performance candidate, not a completion claim.

## Source and host gates

An optional 256 KiB buffer inflated one small tile once for both parse passes.
Default/oversized/allocation-failure paths kept streaming; no-exception builds
ignored it. Focused compression, actual reserve-failure injection, ASan/UBSan,
five real-archive geometry/property/pixel comparisons and P4 portable-core checks
passed. Eight configured engine CTest groups passed. The source was published
to the approved development branch and OrcSDR pin/lock resolved through the
component manager. Only the OrcMaps dependency entry changed.

Consumer map_pipeline/read_cache CTest passed 2/2, including complete frame
comparisons and releasing the buffer budget. Mandatory canvas/preview and 128 KiB
headroom took priority; actual capacity was included in map reclaim reporting.

First native reconfigure was stopped by the requested/locked/fetched pin guard;
the component manager retained b59d478. Running update-dependencies resolved 7ed0d32
without changing other dependencies. No guard or managed source was bypassed.

Native source 3fcd29e3 build and inspection passed. Application 5,259,296 bytes,
headroom 1,032,160 bytes, SHA256
29321dd1d383f75b007e85d9a7ccbe71e209029c7c8154d91c6434c1bb97f802.
40 KiB DMA reserve and C6input unchanged. P4 app-only write, fresh non-app backups,
partition preservation and startup passed. SDKCONFIG before/after was identical.

## Hardware rejection

The first completed frame took 7.882596 s. Subsequent seven-second samples repeatedly
found the worker busy. Ten pans were issued; eight status samples were busy.
The intended 20-pan comparison was aborted deliberately;
no valid candidate p50/p95/max can be computed from those incomplete samples.
After commands stopped, the final completed six-tile frame took 8.462393 s,
stream accounting 4.596983 s, render accounting 3.835391 s, read accounting 0.379740 s,
cooperative wait 1.249719 s (overlapping other accounting). All six tiles used the
262,144-byte buffer. Missing/failed tiles remained 0. Free PSRAM 1,434,584 bytes,
largest 1,376,256; largest DMA 19,456; main-stack high-water 1,432. No reset or
reported RF drop/overrun occurred; sample rate about959 kS/s.

The accepted baseline's 20-pan p50/p95/max are 3.65/4.55/5.13 s. Live RF/startup
conditions vary: this is not an isolated causal multiplier. The new candidate
did not demonstrate improvement or meet the 2-second p95 gate, so it was rejected.
No new optimization is justified by the host passes alone.

Native reclaim released exactly 1,635,728 bytes: two 555,720-byte map snapshots,
262,144-byte read cache and262,144-byte inflate buffer. The LoRa buffers remained
protected while its band was active. This bounded ownership gate passed even
though performance failed.

## Restoration and source disposition

Restored accepted application 30a00ce1, SHA256
8edab674b2ebe3bf8d92e13fc562291ba4e8359bd0feaa77dce16c9c3de65506.
Fresh NVS/PHY/coredump/world backups, boot/table and all non-app preservation
checks pass again. C6 remains unchanged. Native startup reaches Ready. LoRa map
restoration check exits0 with four completed pans 3.232814,3.461930,3.948520 and
3.342874 s; missing/failed 0. This is a bounded recovery check, not a replacement
20-sample percentile benchmark. Receiver point E7 440628866,-1230160273 and labels
picker=1/lora=0/adsb=1/ft8=1 remain unchanged. End state: LoRa map, 906.875 MHz, manual
421 requested, STREAMING about 958 kS/s, drops=0/overruns=0; SD logger ready=true/error=false,
queued=0/dropped=0. PSRAM free 1,698,828, largest 1,638,400; largest DMA 19,456;
main-stack high-water 1,448; no unexpected reset.

Consumer change reverted in ff31e14b; app source is byte-identical to30a00ce1.
Engine optional API withdrawn in 7125ec1; its MVT header/parser/tests match b59d478.
Historical experiment source, binaries, backups and logs are retained. The engine
review lane is https://github.com/hardcoreerik/OrcMaps/pull/7. No main merge or
release occurred. The2-second refill and approximately 33 ms motion gates remain open.

## Reproduction and saved logs

All app commands were run from F:/AI/OrcSDR-TEMP/maps-integration-completion and
all logs are beside invocation:

```powershell
python .local/maps/build_single_inflate_20261023.py reconfigure
python .local/maps/build_single_inflate_20261023.py update-dependencies
python .local/maps/build_single_inflate_20261023.py build
python .local/maps/inspect_single_inflate_20261023.py
python .local/maps/com17_label_bench.py maps-single-inflate-preflash-plan-20261023.json maps-single-inflate-preflash-device-20261023.log
python .local/maps/flash_single_inflate_20261023.py
python .local/maps/com17_label_bench.py maps-single-inflate-pan-plan-20261023.json maps-single-inflate-pan-device-20261023.log
python .local/maps/com17_label_bench.py maps-single-inflate-quiescent-plan-20261023.json maps-single-inflate-quiescent-device-20261023.log
python .local/maps/com17_label_bench.py maps-single-inflate-reclaim-stop-plan-20261023.json maps-single-inflate-reclaim-stop-device-20261023.log
python .local/maps/restore_after_single_inflate_20261023.py
python .local/maps/com17_label_bench.py maps-single-inflate-restored-check-plan-20261023.json maps-single-inflate-restored-check-device-20261023.log
```

Reconfigure exit 2 is retained; dependency update/build/inspection/flash/reclaim/
restoration/check exit 0. Pan plan was intentionally terminated; its console and
raw log remain. Native observer logs: maps-single-inflate-boot-native-monitor-
20261023.log and maps-single-inflate-restore-boot-native-monitor-20261023.log.
Artifact directories .local/maps/maps-single-inflate-7ed0d32-20261023 and
.local/maps/maps-single-inflate-restoration-20261023 preserve manifests/hashes,
recovery references and fresh partition backups. App source was never recovered
by editing managed_components.

Engine checkout holds maps-single-inflate-{host-ctest,allocation-final,asan,
global,p4-final}-20261023.log and maps-single-inflate-withdrawal-compression-
20261023.log. The last focused default-parser run passes after withdrawal;
documentation-truth also reports 0 errors/0 warnings. Earlier sanitizer/equivalence
results qualify the withdrawn code, not current performance acceptance.

## Next gate

Retain the smaller accepted renderer. Inspect duplicate work before another
candidate: the existing completed snapshot may allow reuse of unchanged map
regions and decoding only newly exposed tiles for eligible pans. Any such work
must preserve pixels, label behavior, cancellation, memory and UI ownership;
fallback to complete rendering when reuse cannot be proven. Do not silently
reduce map detail or remove radio servicing to hit the target.

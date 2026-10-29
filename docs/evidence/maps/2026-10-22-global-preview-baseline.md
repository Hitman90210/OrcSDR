# Installed global pack: preview baseline under LoRa reception

COM17, USB serial `30:ED:A0:E2:E6:95`, app `ef53789f`; no reset, flash or
camera mutation in this observation. COM39 is untouched.

From `F:/AI/OrcSDR-TEMP/maps-integration-completion`:

```text
python .local/maps/com17_label_bench.py global-pan-baseline-plan-20261022.json global-pan-baseline-device-20261022.log
```

Exit zero; saved console: `global-pan-baseline-console-20261022.log`. The plan
uses only authenticated status/health commands and verifies active streaming.

At observation the map is active at camera zoom 11/source zoom 8, follow off.
This differs from the preceding zoom-9 offline test, indicating intervening user
interaction. A physical-availability question is pending before any controlled
pan sequence; do not attribute these accumulated counters to a scripted gesture.

| Accumulated preview metric | Same scale, 41 samples | Scaled, 14 samples |
| --- | ---: | ---: |
| Paint call p50 / p95 | 17,943 / 26,155 us | 62,642 / 70,354 us |
| Request feedback p50 / p95 | 18,245 / 26,461 us | 62,969 / 70,625 us |
| Clear call p50 / p95 | 4,783 / 6,680 us | 7,685 / 11,294 us |
| Transfer call p50 / p95 | 13,040 / 23,903 us | 54,977 / 62,654 us |

There is one recorded preview-lock miss. Raw present intervals include user
dwell, and `physical_touch_latency=0`; these are not end-to-end gesture latency
or FPS measurements. Same-scale paint costs are encouraging, but scaled preview
cost exceeds 33 ms. The representative interaction acceptance gate stays open.

The completed zoom-11 frame covers two tiles, 3,412 features and 418,760 bytes in
212 reads, no missing/failed tiles. Detailed render total is 2,833,589 us,
including 919,312 us IO; cached push is 27,528 us. Its smaller footprint prevents
a direct speed comparison with the six-tile zoom-9 offline observation.

Driver: streaming about 960 ksps at 906.875 MHz, reported drops/overruns zero.
Health: uptime 761,108 ms; free PSRAM 2,077,832 bytes, largest 1,966,080;
internal free 22,395; largest DMA 8,192; main stack high-water margin 1,096.
No additional memory allocation or firmware change was needed for this capture.

Next gate: an isolated, controlled pan/zoom sequence with the real global pack
under reception, followed by physical acceptance. Preserve separate paint,
feedback, detailed redraw and actual gesture timing claims.

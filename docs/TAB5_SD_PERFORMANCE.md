# Tab5 SD Write Performance

## Scope

This investigation measured the Tab5 microSD write path without changing the
SD wiring, ESP-Hosted firmware, or radio sample rate. The benchmark creates only
`/sd/orcsdr/sdbench.bin`, flushes and closes it, and removes it after every run.
It never formats the card or writes raw sectors.

Tested hardware and software:

- Branch: `codex/shortwave-dashboard-completion`
- Source base: `4e4174e3162e88d97d86073de082ea632193857d`
- ESP-IDF: 5.5.4
- Tab5 SD: SD32G SDHC, native 4-bit, 40.00 MHz, 512-byte sectors
- SD pins: CLK 43, CMD 44, D0 39, D1 40, D2 41, D3 42; LDO4
- ESP-Hosted C6 SDIO clock: unchanged at 10 MHz
- RTL-SDR: Blog V4 (`blog_v4_r828d`) with MLA-30+

## Root cause

The original storage wrapper forced every writable `FILE` to `_IONBF`. Removing
that setting and selecting `CONFIG_FATFS_VFS_FSTAT_BLKSIZE=4096` did not improve
large writes: every tested size remained near 0.30 MiB/s.

The decisive bottleneck was the ESP32-P4 SDMMC DMA-alignment contract. The
high-volume source buffers lived in PSRAM but were not aligned to the runtime
cache-line requirement (128 bytes on the tested device). ESP-IDF consequently
used its unaligned fallback, copying and writing one 512-byte sector at a time.
An aligned diagnostic buffer immediately raised throughput into the 4-5 MiB/s
range.

The production correction is deliberately small:

- remove forced `_IONBF`;
- give writable storage files a cache-aligned 32 KiB PSRAM stdio buffer;
- cache-align the existing audio and IQ capture buffers;
- set `CONFIG_FATFS_VFS_FSTAT_BLKSIZE=4096`.

No producer/consumer queue was added. Audio and IQ are already captured into
PSRAM and exported only after acquisition stops, so a second queue would add
memory and synchronization without removing a live producer stall.

## Benchmark results

Each value is durable throughput measured after `fflush` and `fclose`. The 32
KiB and 64 KiB values are three-run averages; other sizes use one run.

| Write size | Baseline | `_IONBF` removed only | Final production | Improvement |
| --- | ---: | ---: | ---: | ---: |
| 4 KiB | 0.300 MiB/s | 0.298 MiB/s | 3.872 MiB/s | 12.9x |
| 16 KiB | 0.296 MiB/s | 0.301 MiB/s | 3.878 MiB/s | 13.1x |
| 32 KiB | 0.297 MiB/s | 0.299 MiB/s | 4.526 MiB/s | 15.2x |
| 64 KiB | 0.297 MiB/s | 0.300 MiB/s | 4.541 MiB/s | 15.3x |
| 128 KiB | 0.296 MiB/s | 0.302 MiB/s | 4.163 MiB/s | 14.1x |

32 KiB was selected because its final durable average is within 0.4% of 64 KiB
while consuming half the per-file buffer memory. The final 4 KiB and 16 KiB
results also prove that the shared stdio buffer aggregates small application
writes effectively.

At 2.4 MSPS, unsigned 8-bit interleaved IQ requires approximately 4.58 MiB/s.
The old path provided only about 6.5% of that rate. The corrected path is close
to the raw-IQ requirement, but continuous raw-IQ recording is not accepted by
this benchmark: filesystem variance, headers, competing tasks, and safety
margin still require a dedicated sustained-capture test.

## Repeatable commands

Focused host checks:

```powershell
wsl.exe bash -lc "set -e; cd /mnt/f/Ai/OrcSDR/.codex-worktrees/shortwave-dashboard-completion; g++ -std=c++17 -Iapps/orcsdr-tab5/ui tests/sd_benchmark_plan_tests.cpp -o /tmp/sd_benchmark_plan_tests; /tmp/sd_benchmark_plan_tests"
& .\apps\orcsdr-tab5\tools\run-shortwave-ui-regression.ps1
```

Build:

```powershell
& .\apps\orcsdr-tab5\tools\build-tab5-idf.ps1
```

Authenticated on-device benchmark:

```powershell
& .\apps\orcsdr-tab5\tools\run-tab5-ui-regression.ps1 `
  -Port COM17 -SdBenchmark -SdBenchmarkMiB 32 `
  -PairingKeyPath <path-to-local-pairing-key> `
  -LogPath .\artifacts\sd-benchmark\production-buffered.log
```

## Final candidate evidence

- Application: `apps/orcsdr-tab5/build-native-hosted3/orcsdr_tab5.bin`
- Size: 2,478,128 bytes
- SHA-256: `402BBC2E2E6CA94DE9532DE1795CB3E5A9416188A5CDE2CA39A8DA06E3959BF0`
- Flash: COM17, ESP32-P4 revision 1.3; bootloader, partition table, and application hashes verified
- Radio serial state: streaming, V4 profile, 30.000 MHz tuner route,
  2,399,931 effective samples/s, zero USB overruns and zero consumer drops
- Recording serial state: 260,178 mono PCM samples at 48 kHz (5.42 seconds)
  written to `/orcsdr/rec_001_SHORTWAVE_30000000.wav`

These are separate claims. Build, flash verification, SD benchmark, radio
telemetry, and successful WAV export passed. Audible reception, physical UI,
and sustained continuous 2.4-MSPS raw-IQ recording were not tested for this
final candidate.

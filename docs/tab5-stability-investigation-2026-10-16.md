# Tab5 FM and Wi-Fi stability investigation

Do not treat the earlier Wi-Fi crash as resolved solely because
the repeat tests do not crash. The saved dump establishes a null Wi-Fi input
callback, but omits the interface's heap object and therefore does not establish
why that callback became invalid.

## Reproduced resource failure

Repeated Settings Wi-Fi off/on/connect actions plus a location lookup left a
fragmented internal DMA heap. A subsequent FM/Home run had sound enabled and RF
sampling/decoding active, but `speaker_running=0`, `audio_chunks=0`, and every
audio/RF drop counter remained zero. Five consecutive status samples preserved
this failure in `.local/stability/fm-diag-run3/`.

A temporary speaker guard reduction from 3072 to 2304 bytes allowed startup
with the configured 256-frame buffer, but did not address Wi-Fi allocation
shortages. It was tested and then removed; the original guard remains intact.

## Production candidate

The SDIO packet allocation path uses `eh_host_port_dma_alloc`, which always
requests ordinary DMA memory. The existing aligned allocator honors the configured
`CONFIG_EH_HOST_PORT_DMA_PREFER_SPIRAM` preference and falls back to internal DMA.
The candidate changes the SDIO non-pool path to request 64-byte aligned buffers
through that allocator. Other SDIO paths already use it. This preserves internal
DMA for the speaker and network setup and gives packet buffers the same alignment
as the existing transport paths.

This is a one-line patch against the pinned ESP-Hosted 3.0.6 component,
`e1a4f5492ac44fa248d77ac971d2055f8c565442`, installed after IDF configure by the
existing patch helper. No dependency versions or C6 image were changed.
Upstream source and license: [Espressif ESP-Hosted](https://github.com/espressif/esp-hosted),
Apache-2.0; the original source license header remains in place. The patch is
derived from that file, rather than an independent replacement driver.

The host regression test compiles the actual installed `sdio_buffer_alloc`
function with allocator stubs. It failed before the change and passed afterward,
checking allocator selection, alignment/size, optional zeroing and allocation
failure handling:

From the repository root in Linux or WSL (after native IDF configure):

```sh
python3 tools/test-hosted-sdio-dma.py
# HOSTED_SDIO_DMA_TEST_PASS

bash tools/test-tab5-cadence-history.sh
# home_cadence PASS; waterfall_style PASS; lora_history PASS
```

## Test-only receive diagnostics

`build-tab5-idf.ps1 -WifiRxDiag` enables a wrapper around `esp_netif_receive`.
It reports sampled interface/callback addresses and always logs a null callback
before invoking the original receive path. It does not discard or change packet
delivery. A local copy of the interface is retained on the receive task stack so
a future core dump can include it without capturing the entire heap.

The option defaults off, and the build script explicitly sets it on/off to avoid
stale CMake settings. These diagnostics are not a fix for the null callback.

## Evidence and acceptance boundaries

On COM39 with its V3c dongle, the diagnostic candidate passed six Settings
Wi-Fi off/on/connect cycles while receiving LoRa at 906.875 MHz. Two cycles
included an IP location lookup and completed certificate validation. The log
contains six completed cycle-health records, no SDIO allocation-shortage
warnings, no null-callback observations and no panic/reset markers. This is
bounded reproduction evidence; it does not establish the cause of the saved
null-callback crash.

The final normal build has `ORCSDR_WIFI_RX_DIAG=0`; its ELF contains no receive
wrapper symbol. Application SHA256:
`321ffd6cd7c368d984cf922c381e809ef731176337fe76ec1ad1daf423d1f383`.
It was flashed to COM39 at application offset `0x10000`; NVS and the C6 image
were not written. The final FM/Home soak uses this normal build, keeps Wi-Fi
connected, and receives concurrent read-only HTTP status/spectrum requests.

Raw dumps, logs, binaries and serial authentication material are not committed.
Evidence is under `.local/com39-crash/` and `.local/stability/` in this worktree.
Some attempted runs ended because concurrent serial output corrupted status JSON
or pairing replies. These are failed test runs, not passing soak results and not
proof of a reset. Later local recorders preserve malformed replies and retry.

The speaker-only trial reached approximately seven minutes with steady audio and
no counted audio/RF drops; it still had Wi-Fi allocation warnings and was stopped
to test the allocator candidate. It is not a substitute for the required final
30-minute soak.

## Final results

The normal candidate completed 1,860.18 seconds (31 minutes) of FM at 96.1 MHz
on HOME with Wi-Fi connected. All 186 samples reported the speaker running;
audio production ranged from 22.810 to 24.252 chunks/s. Audio, ring, submission
and USB drop counters stayed zero, with no runtime fault markers or status
probe timeouts. Concurrent HTTP status/spectrum traffic completed 1,596 requests
successfully, with zero failures. No final-minute audio slowdown recurred.

HOME delivered 23.417–24.102 updates/s: sampled p50 intervals were 42–43 ms and
p95 intervals 46–53 ms. The requested refresh interval remains 33 ms; this does
not mean actual rendering reaches 30 frames/s. Free heap started at 6,060,956
bytes and ended at 6,060,872 bytes, with a range of 6,057,660–6,061,216 bytes.
The test restored LoRa at 906.875 MHz and sound off without resetting the device.

On October 17 the user reported that COM39 ran overnight and was saving and
working correctly. A read-only serial check confirmed uptime 46,182,451 ms
(12.83 hours), LoRa still selected, Wi-Fi connected, and zero reported audio
drops. Free heap was 2,159,936 bytes. This overnight LoRa observation has no
matched post-switch heap baseline and is not a memory-leak acceptance result.

The FM soak gate passes for this setup. The reproduced SDIO allocation pressure
and resulting silent speaker startup are addressed. The saved Wi-Fi crash's
null callback remains unexplained; successful stress and overnight operation
are encouraging evidence, not proof of its root cause or permanent resolution.

## October 17 pre-merge checks

Seven read-only samples over 180 seconds, after uptime reached 13 hours,
reported exactly 2,159,936 bytes free heap throughout. The device cache held
86 nodes; the live write counter advanced from 698 to 699 without queue drops
or logging errors. The cache itself has fixed storage for 256 nodes and a
128-entry duplicate filter. This short observation does not prove the absence
of a slower leak or explain the difference from the FM memory footprint.

Reboot exposed a persistence defect: replay restored only the earlier 85
records/26 nodes, and recent archive files were zero bytes. The prior tests
stopped logging before reset, closing the files and concealing this failure.
`File::flush()` calls `fflush`, while this firmware disables
`CONFIG_FATFS_IMMEDIATE_FSYNC`. FAT data/directory metadata was not explicitly
committed while the files remained open. A successful live write counter is
therefore not evidence that the overnight records survived. Existing files
are preserved; no recovery of the missing records is claimed.

The candidate adds an explicit `File::sync()` (stdio flush plus filesystem
`fsync`) and uses it only for the LoRa worker's five-second journal/CSV commit.
Other users of `flush()` retain their behavior. The SD semantics check also
checks directory metadata after sync, then closes the writer before reading
the payload. This avoids a duplicate open of a writable FAT file.
The COM39 normal build with this change has SHA256
`53568e016f4c6e5e392a492e3935cf069cbf4822d0aa868de472fcda15e7bd9c`.
Native build and application-only flash passed. The authenticated
`RTL_SD_SELF_CHECK` passed, including `sync=1 synced_read=1` while the writer
was still open. Three RF text packets from Heltec COM24, IDs `d84be191`,
`35f87992`, and `8ce7a193`, matched the synced CSV. After authenticated
`RTL_RESET` without `LORA_SD_LOG OFF`, replay increased from 85 to 88 records
with zero rejects; all three packet IDs remained in CSV readback. Logging and
LoRa reception were restored. Evidence: `.local/stability/open-log-reset.log`
and before/after CSV snapshots. This validates software reset after sync;
physical power-cut/card-removal durability remains untested.

External review identified the duplicate-open issue in the first version of
the self-check. That version passed with this build's `CONFIG_FATFS_FS_LOCK=0`,
but did not respect the FatFs open contract. The final test uses `stat` before
closing and reads afterward. The repeat RF reset test likewise performs CSV
readback only after reboot and stopping logging. The production periodic-sync
behavior is unchanged by this test correction.

The review's request for automatic retry after sync failure is optional here:
the worker sets the SD error flag and stops on a failed barrier; it does not
claim those records are durably committed. `written` counts completed writes,
as documented, and `LORA_SD_LOG ON` is the explicit retry. No automatic retry
or recovery guarantee on failed/removable media is added.

The final self-check correction was rebuilt/flashed successfully (application
SHA256 `27de88653a5508913a72a1e1ed05693cd4a20c54bc10756ed9c59bfb6c6a3ffc`).
Its hardware self-check reported `sync=1 synced_metadata=1 read=1 pass=1`.
Three further controlled RF packets (`95b7e2ec`, `d70d66ed`, `95e70eee`)
survived reset with logging left open: replay increased from 100 to 103 with
zero rejects. Logging was stopped only after reboot for CSV readback, which
matched all three IDs, then logging and LoRa reception were restored.
Evidence: `.local/stability/open-log-reset-final.log`.

Before that final test-only rebuild, a post-navigation 240.33-second sample
held free heap within 1,968,832–1,968,852 bytes while written records advanced
from 2 to 6. Nine samples reported no logging errors or drops. Firmware and
screen state differ from the overnight run, so this is not a like-for-like
comparison of their free-heap values.

On this firmware, all 20 HOME/RF Lab navigation cycles passed (Home command,
RF Lab close, header touch, and keyboard Escape paths), with five Settings
round trips. Each return showed RF Lab inactive, HOME selected, and advancing
plot frames. `UI_SELF_CHECK` passed all reported components;
`RTL_UI_REGRESSION RUN` and subsequent `CHECK` both reported `pass=1`.
The first attempted navigation run on the preceding CSV-only candidate began
before staged radio startup finished and failed its plot-progress assertion;
the successful run explicitly waited for running capture.

Final targeted commands passed: `bash tools/test-tab5-cadence-history.sh`
(cadence, waterfall, history/CSV under ASan/UBSan) and
`python3 tools/test-hosted-sdio-dma.py`. CSV formula prefixes are neutralized
only on new exports; archive text remains unchanged. Full suites and suitable-
signal P25 acceptance were not run for these narrow changes.

# LoRa logger automatic recovery repair - October 26, 2026

The old task removed queued records even after a logging failure had latched its
error flag. The focused production-task fault test reproduces this loss before
the source repair. This repairs that behavior; it does not establish the cause
of the original SD error or restore the nine historical missing records.

## Source change

- The worker retries failed storage operations after two seconds, yielding while
  waiting. It does not restart a FreeRTOS task for each received packet.
- One current record is held in PSRAM until both journal and CSV writes sync.
  The existing 32-record queue remains intact behind it. Only actual queue
  overflow increments dropped records; a write failure retains the current row.
- A pending append remembers both file boundaries. A retry reopens the same
  journal/CSV in update mode and rewinds only the current record's uncommitted
  suffix. Successful earlier rows are preserved, and partial writes or failed
  syncs do not create duplicate rows on retry.
- Unexpected file sizes prevent truncation rather than deleting unowned data.
  Permanent storage failure remains visible while the queue is held.
- Each completed record now syncs both files rather than batching a five-second
  sync. The cost of this durability change needs reception testing on COM17.
- OFF suspends error retries; ON signals the worker to retry immediately, after
  closing failed streams. The UI does not clear the error under a live writer.
- `LORA_SD_LOG STATUS` adds pending, retry_attempts, last_error_stage and
  last_errno. `LORA_SD_LOG_ERROR`, `LORA_SD_LOG_RETRY` and
  `LORA_SD_LOG_RECOVERED` identify the failing operation and automatic recovery.
- Driver, Wi-Fi/C6, DSP, map rendering and map downloads are outside this change.

## Verification

`bash -lc 'python3 tests/lora_log_recovery_test.py'` exercises the production task
and real serializers, with fault injection through the storage API. All 19 cases
pass with AddressSanitizer/UndefinedBehaviorSanitizer:

short journal/CSV writes, a partial CSV header, journal/CSV sync failures, CSV and
journal open failures, permanent failure, logging OFF, unknown extra suffix data,
queue overflow, timer wraparound, failed rollback, transient stat error, OFF/ON
recovery, and failures on later records preserving previously committed rows.
The first test fails on the old task because it discards the current and queued
records rather than recovering. The original red log is retained.

Existing storage update-mode and nine FT8 log-flush fault checks also pass.
Cadence/history/lifecycle and LoRa handoff/startup checks are run separately.

Raw logs beside invocation in
`F:/AI/OrcSDR-TEMP/maps-integration-completion`:

- `lora-log-recovery-red-actual-20261026.log`
- `lora-log-recovery-19-cases-20261026.log`
- `lora-log-recovery-focused-host-20261026.log`
- `lora-log-recovery-cadence-host-20261026.log`
- `lora-log-recovery-final-host-20261026.log`

Native build and COM17 app-only validation passed for source
`fd8b6de623e5f85ee28534e8ac4e8db6eb87ca21`. App SHA-256:
`3c700d175e9d5f35f507bbb3d8c836c870b2fe61f6623df8d6ccf3987350b5e1`.
The app is 5,306,656 bytes with 984,800 bytes headroom. The retained record
and journal path occupy 276 bytes in PSRAM; the 40 KiB DMA reserve is unchanged.
NVS, SD data, world partition and C6 were preserved. COM39 was not flashed.

COM17 naturally reported `journal_open errno=2` at 15:05:18.876 PDT,
retried at 15:05:20.869 with one pending record, and reported recovery at
15:05:21.945. This proves recovery from an actual open failure, not its
underlying filesystem cause. The held packet (4109905239) was saved exactly
once. All ten records in the new journal matched exactly one CSV row, and
the complete preceding 218,738-byte CSV prefix was unchanged. An additional
controlled NODEINFO packet (2457770009) was also saved once in both files.

The initial strict serial-only packet verifier failed: one NODEINFO transmission
was not observed on either receiver, and serial capture omitted a text that
the saved CSV proves was received. These are retained as test limitations,
not reported as a successful six-packet serial comparison. The additional
COM39 reference capture could not open its occupied port; no owner was killed.

A stationary 144,657 ms reception window at 960,000 samples/s recorded zero
IQ pipeline drops, driver drops or overruns. Final COM17 status was streaming,
logging ready, error false, queue/pending/dropped all zero, retry count one.
The 19 focused recovery cases passed, including partial writes, sync/open
failures, retry timing, OFF/ON, queue overflow and protected suffix rollback.

Additional logs beside invocation:
`release-review-native-lora-recovery-20261026.log`,
`lora-recovery-artifact-inspection-20261026.log`,
`lora-recovery-live-com17-20261026.log`,
`lora-recovery-final-readback-com17-20261026.log`.
CodeRabbit's incremental review of this source was still pending at this
checkpoint; earlier clean comments apply to the previous source revision.

No physical card-failure or power-cut durability pass is claimed.
Pending RAM data is not nonvolatile storage, and sustained unavailability can
fill the bounded queue. The earlier watchdog incident remains open separately.

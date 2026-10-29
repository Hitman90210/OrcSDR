# LoRa dropped-record investigation - October 26, 2026

This investigation reconciles the COM17 V4 incident with recorded reception and
saved files. It does not claim the initial logger failure has been fixed.
Firmware source remains `380e00ea`; no firmware or C6 changes were made.

## Nine records identified

The incident serial trace contains nine distinct received sender/packet-ID pairs.
All nine are absent from the saved CSV snapshot taken after the incident:

| Record type | Count |
|---|---:|
| Controlled E1/E2/E3 text packets | 3 |
| Controlled NODEINFO packets | 3 |
| Position packet | 1 |
| Telemetry packet | 1 |
| Unknown/encrypted-frame packet | 1 |

This is not nine text messages, nine RF losses or nine independently established
SD-card write errors. The exact identities and captured contents are preserved
in the local audit, not republished in Git. Reception passed; persistence failed.

## What the dropped counter actually counts

`enqueue_lora_sd_log()` increments the counter when its 32-record queue rejects
a new record. Separately, `lora_sd_log_task()` increments it for any dequeued
record that cannot complete the journal/CSV append path.

The latter includes records taken from the queue while `lora_log_error` is
already true: `ok` starts false, no write is attempted, and the record is still
removed and counted as dropped. The error closes both files and prevents
reopening until logging is explicitly enabled again. Thus one initial failure
can discard subsequent otherwise valid records; the count cannot identify the
initial operation or prove multiple independent media failures.

The task also sets the error flag on sync/close/replay failures without assigning
that pending batch's size to the dropped counter. The aggregate is therefore
not a complete measure of durable records lost under every failure mode.

There were nine distinct packets in the incident trace and the queue capacity
is 32. The evidence does not justify increasing queue capacity or the memory
pool as a remedy. The exact first failed operation was not logged by this build.

## Journal inspection

COM17 reception was stopped briefly for read-only transfers and resumed after
each capture. The following journals were downloaded with protocol checksums:

- `00000021-20261026.jsonl`: 7,686 bytes; 26 valid records and two invalid
  records starting at offsets 7,141 and 7,207. There are 222 non-ASCII bytes,
  all in zero-based 512-byte sector 14. The journal serializer emits ASCII JSON
  and hex-encoded strings, so these bytes are unexpected file contents.
- A second read of journal 21 was byte-identical. SHA-256:
  `8d0adcd9c37a5c33c8da224c5e9c0a4868201d1c1b8f32e8fc282cfeff86f42d`.
  This is not explained by the host's earlier text-decoding failure alone.
- `00000023-20261026.jsonl`: 6,653 bytes at capture, 24 valid records, no
  non-ASCII bytes or invalid records.
- None of the nine incident IDs appears in either checked journal.
- A read of the expected journal-22 path returned `SD_GET_ERROR open_failed`.
  That result alone does not distinguish absence, a directory or another open
  failure. No file was removed, renamed, repaired or overwritten.

The damaged journal tail is compatible with an interrupted write but does not
prove that explanation or identify the event that disabled logging afterward.
Do not attribute the initial failure to a dongle, low memory or a defective card
from this evidence alone.

The nearby SDIO errors in the startup log are from C6 Wi-Fi transport operations.
They are not proof of SD-card errors. The configured IDF 5.5 Hosted code uses
slot-specific teardown; inspection does not establish a whole-controller reset
as the cause. No Wi-Fi/driver workaround was applied.

## Reproduction and evidence

Invocation directory: `F:/AI/OrcSDR-TEMP/maps-integration-completion`.

Command: `python .local/maps/audit_lora_drops_20261026.py`.
Actual result: exit 0, nine received IDs absent from the CSV and two checked
journals, with the expected 3/3/1/1/1 type counts. Repeat-read equality is asserted.

Local artifacts, outside Git:

- `.local/maps/lora-dropped-records-audit-20261026.json` (packet identities and contents)
- `lora-dropped-records-audit-20261026.log`
- `lora-drop-journal-read-com17-20261026.log`
- `lora-drop-journal-verify-com17-20261026.log`
- `.local/maps/lora-drop-journal21-20261026.jsonl`
- `.local/maps/lora-drop-journal21-repeat-20261026.jsonl`
- `.local/maps/lora-drop-journal23-20261026.jsonl`
- `release-v4-com17-rf-retry-20261026.log`
- `.local/maps/release-v4-com17-lora-saved-20261026.csv`

The serial capture preserves the missing packet evidence; nothing was silently
reconstructed into device history or counted as saved RF data. Final COM17
state is STREAMING, logging ready, no error, queue empty and dropped count 9.

Remaining work: expose the first failing logger stage/error code, reproduce it,
then correct recovery without discarding queued data or duplicating partial
writes. A manual logging restart is not a validated permanent fix.

Follow-up: source `fd8b6de6` now retains pending records and retries automatically.
Host fault tests and an actual COM17 open-error recovery passed; see
[repair validation](2026-10-26-lora-logger-recovery-fix.md). The nine historical
missing records were not reconstructed into device storage.

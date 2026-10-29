# LoRa packet and device history

Shared SD buffering, append/durability requirements and regression commands are
tracked in [SD card reliability](SD_CARD_RELIABILITY.md).

The node observation model follows Meshtastic's documented distinction between
received packets and a changing node database:
[Meshtastic Python API](https://python.meshtastic.org/). The archive and cache are
independently implemented here; no upstream implementation or new dependency is
included.

With an SD card mounted at boot, LoRa logging starts automatically. `LORA_SD_LOG
OFF` stops logging for the current boot; `LORA_SD_LOG ON` starts it or requests
an immediate retry. The worker also retries SD errors automatically every two
seconds while logging is enabled. `LORA_SD_LOG STATUS` reports ready/error,
queued and pending records, retry attempts, last error stage/errno and drops.

Two files serve different purposes:

- `/orcsdr/lora_packets_v2.csv` is a spreadsheet-friendly packet log, including
  decoded text, short and long names, encryption status, and host-injection status.
  Existing `lora_packets.csv` files are preserved because their header differs.
  New CSV writes prefix formula-like text/name cells with an apostrophe for
  spreadsheet safety; previously written rows are not rewritten. The replay
  archive retains the original decoded text.
- `/orcsdr/lora/00000001-YYYYMMDD.jsonl` and subsequent sequence numbers are the
  replay archive. Each boot, restart of logging, or UTC day change starts a new
  file. An unavailable clock produces an explicit `undated` filename. Sequence
  order preserves receive order even when the clock becomes valid or changes.

The archive records the existing decoded packet fields, not raw RF captures or
complete protocol payloads. Names and text use hexadecimal UTF-8 bytes in JSON to
preserve quotes and line breaks safely; CSV contains readable text. Strings retain
the existing decoder limits (7-byte short name, 31-byte long name, 111-byte text).
Payloads that cannot be decoded retain metadata only. The `encrypted` field
describes encryption on air, including packets successfully decoded using the
built-in public channel key or an authorized receive key. Decoded text, NODEINFO,
and positions from those packets can therefore be saved even when this flag is 1.

On startup, archive replay reconstructs the most recent 256 RF senders in memory.
The Nodes and Map views display up to their existing 16-node limit. A device keeps
its latest NODEINFO names and valid position when later packets omit those fields.
Historical observations stay in the archive, including earlier names. Host-injected
records are labeled and excluded from the RF device list. Device IDs and names are
observations, not authenticated identities. A record from an earlier boot displays
`SAVED` instead of calculating its age from the previous boot's uptime.

`LORA_HISTORY STATUS` reports replay completion, records read/rejected/written,
node count, and the SD error flag. `LORA_HISTORY NODE 1234abcd` reports a cached
device's first/last UTC, packet count, and retained names/position. Zero UTC means
unknown. Counts use a bounded recent-packet duplicate filter, not a lifetime exact
packet ledger; an evicted device's summary is rebuilt if it is heard again.

Stop reception with authenticated `RTL_STOP`, then stop logging before copying
files. `LORA_HISTORY FILES` lists archive paths; `SD_LIST` lists the CSV. Existing
`SD_GET_*` transfers can retrieve these files with hash verification. Archive files
must be retained to reconstruct the device list. No automatic deletion is performed.

Writes run on the logging worker. It keeps one pending record until both its
journal and CSV writes have synced, then takes the next queued record. After a
failure it closes the failed streams, waits two seconds and retries while keeping
the pending record and queue intact. Partial writes are rewound only within that
record's saved append boundaries before retry, preserving older rows and avoiding
duplicate records. Unexpected file sizes block recovery instead of truncating
unowned data. Error stages and errno are printed as `LORA_SD_LOG_ERROR`;
`LORA_SD_LOG_RETRY` and `LORA_SD_LOG_RECOVERED` expose automatic recovery.

The queue holds 32 records in addition to the pending record. Queue overflow is
counted as dropped without blocking decoding; a transient write failure itself
is retried rather than counted as a lost record. Disabling logging pauses error
retries until it is enabled again. `written` counts records synced to both files.
The per-record sync cost must be checked under reception. Pending RAM data is not
power-loss storage, and physical card failure can still prevent saving. Replay
rejects malformed or incomplete final lines and continues with subsequent files.

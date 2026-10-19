# LoRa packet and device history

The node observation model follows Meshtastic's documented distinction between
received packets and a changing node database:
[Meshtastic Python API](https://python.meshtastic.org/). The archive and cache are
independently implemented here; no upstream implementation or new dependency is
included.

With an SD card mounted at boot, LoRa logging starts automatically. `LORA_SD_LOG
OFF` stops logging for the current boot; `LORA_SD_LOG ON` restarts it or retries
after an SD error. Status and dropped records are reported by `LORA_SD_LOG STATUS`.

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

Writes run on the logging worker, with a five-second stdio/filesystem sync interval. Sudden power
loss can lose pending writes. Replay rejects malformed or incomplete final lines
and continues with subsequent files. A short write or flush failure sets the SD
error flag; `written` counts completed writes, not a guarantee against power loss.
Queue overflow increments the dropped counter without blocking decoding. SD-full,
physical card removal, and power-cut durability need separate hardware validation.

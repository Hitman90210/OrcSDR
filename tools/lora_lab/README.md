# OrcSDR LoRa validation lab

`run_suite.py` records a controlled three-view baseline:

`Meshtastic TX -> Meshtastic reference RX -> OrcSDR RX`

It does not change radio configuration. Before transmitting, it verifies that
both Meshtastic devices already use the same US LongFast primary channel and
that OrcSDR reports US LongFast with automatic capture enabled. Channel keys
are compared only in memory and are never written to evidence.

Run the host-only check:

```powershell
python tools/lora_lab/run_suite.py --self-check
```

Record a serial inventory. Optional role ports are opened only long enough to
read identity/configuration; failures are recorded instead of aborting the
inventory:

```powershell
python tools/lora_lab/run_suite.py --inventory-only `
  --tx-port COM24 --reference-port COM16 --orcsdr-port COM17
```

Run a legal local over-the-air baseline after identifying all three ports:

```powershell
python tools/lora_lab/run_suite.py `
  --tx-port COM24 `
  --reference-port COM16 `
  --orcsdr-port COM17 `
  --transport OTA `
  --confirm-local-ota-legal
```

Use `SHIELDED_RF` or `CABLED_RF` only when the physical setup actually provides
that isolation. The harness intentionally has no switch that bypasses this
transport distinction.

Each run is saved under `artifacts/lora_validation/<timestamp>/` with raw TX,
reference RX, and OrcSDR logs plus JSON, CSV, configuration, inventory, and a
Markdown summary. OrcSDR correlation is currently a non-overlapping host-time
window; payload-level correlation requires retained IQ or an explicit decoded
packet event and is not claimed by this first harness revision.

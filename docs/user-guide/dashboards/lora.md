# LoRa

The LoRa dashboard is an experimental receive and analysis surface.

- **Overview** shows channel settings, spectrum, capture controls, and recent traffic.
- **Nodes** lists received sender records; missing fields remain unknown.
- **Traffic** retains bounded receive events and packet details. Each row keeps
  monotonic age and, when the hardware clock was established at receipt, a
  fixed UTC timestamp. **TIME NOT SET** is shown when UTC was unavailable. The
  event list uses the full panel width for readable timestamps and messages.
  Its upper toolbar switches between messages and retained packet details,
  saves the recent log, changes the filter, or clears the in-memory events.
- **Map** plots positions only when a packet contains usable coordinates.
- **RF Health** reports receiver rate, drops, capture/log state, and decoder readiness.

LoRa packet capture and Meshtastic interpretation depend on the selected spreading factor, bandwidth, channel, and keys. OrcSDR does not transmit from these pages.

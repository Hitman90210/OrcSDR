# Global Settings

The gear opens Settings without stopping ordinary reception.

| Category | Purpose |
|---|---|
| Connectivity | Wi-Fi power, internal/external antenna, scan, connect, and four saved profiles |
| Location & ADS-B | One receiver location and 10/25/50/100 NM radar range |
| Data & Maps | Aircraft database and offline map-pack status |
| Display & Audio | Brightness, timeout, global volume, sound default, and 180-degree rotation |
| Radio Defaults | Startup reception, last/default band, FM frequency, and graphics |
| Storage | SD health, capacity, and bounded maintenance status |
| Companion | Optional LAN web console for a browser or Android TV, and whether visitors may control the receiver |
| System | Build, uptime, power, and diagnostics |

Saved Wi-Fi passwords are masked and never returned through the Settings UI or documentation capture interface.

Companion → ENABLE starts the companion console at `http://<tab5-ip>/` and
advertises `orcsdr.local`. Use that URL from a browser or the sideloaded
`apps/orcsdr-tv` app.

The page follows whatever the receiver is doing:

| Band | What the page shows |
| --- | --- |
| ADS-B | A radar scope with range rings and a sweep, up to 16 nearest aircraft plotted by range and bearing with heading vectors, plus a nearest-traffic list |
| CB, GMRS, marine, weather | A radio faceplate: large channel readout, RX and BUSY lamps, signal gauge and the channel plan |
| Everything else | Frequency, signal meter and history, live spectrum and waterfall |

**VISITOR CONTROL is a separate switch, and it is off by default.** With it
off the page is genuinely view-and-listen: `POST /api/action` is refused with
403 whether or not the browser ever loaded our page. With it on, anyone who
can open the URL can retune the receiver and change its volume -- there is no
password and no TLS, so turn it on only on a network you trust.

The page never receives the receiver's coordinates. Aircraft are sent as range
and bearing, which is what a radar needs and what keeps your position on the
device. Relative positions are provided only when both the aircraft fix and the
receiver location are available; until a receiver location is configured,
aircraft can appear in the list but are not falsely plotted at the center. Valid
zero values (including 0 ft, 0 kt, 0 degrees, and level vertical rate) remain
distinct from unavailable measurements.

Muting the Tab5 speaker does not mute the browser stream; they are separate.

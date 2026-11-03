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
| Companion | Optional LAN web console for Android TV, plus phone/BLE placeholders |
| System | Hardware UTC status, build, uptime, power, and diagnostics |

Saved Wi-Fi passwords are masked and never returned through the Settings UI or documentation capture interface.

Maps integration builds add **LOCAL MAPS** under **Data & Maps**. The
[offline maps guide](offline-maps.md) explains **GET MAPS**, installed-map
selection and copying a downloaded pack onto the SD card yourself. Availability
of remote downloads depends on the catalog configured in that firmware.

System shows a green UTC value only after the hardware clock has been
established. Otherwise it shows **TIME NOT SET** in orange. With the Tab5
connected by USB, run `python tools/sync_tab5_rtc.py COM17` (replace `COM17`
when needed). This authenticates locally and copies the computer's UTC time;
no network connection is required.
Once established, Home shows UTC; otherwise its uptime clock is labeled
**TIME NOT SET** so the missing wall clock is visible outside Settings too.

Companion → ENABLE starts the TV Mission Control page at `http://<tab5-ip>/` and
advertises `orcsdr.local`. Use that URL from a browser or the sideloaded
`apps/orcsdr-tv` app. Device control is a separate switch: **Companion →
REMOTE CONTROL** reads **VIEW ONLY** by default, and `POST /api/action` is then
refused with `403 control disabled`. Set it to **ALLOWED** (or send
`RTL_WEB CONTROL ON` over serial) and the same endpoint can tune, change volume
or mute state, adjust span/step, and open dashboards. There is no TLS or
application authentication either way, and the Origin check stops only
browsers, so allow control only on a trusted LAN and never expose port 80 to
the Internet.

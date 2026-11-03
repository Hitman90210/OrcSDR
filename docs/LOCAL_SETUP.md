# Making OrcSDR local to you

Several OrcSDR features need data about *where you are*: the ADS-B radar,
the offline map, and P25 trunking. This is how to set each one up for your
area.

Everything here is done from a PC with the SD card, or over Wi-Fi. Nothing
needs a rebuild of the firmware.

## 1. Set your receiver location

**ADS-B dashboard → SETUP tab → latitude / longitude.**

This is the origin of the radar plot and the anchor for the nearest-ATC
lookup. Without it the radar has nothing to centre on and the ATC card reads
`NEEDS YOUR LOCATION`. Use decimal degrees, negative for west/south.

## 2. Data packs (FAA aircraft, FAA aviation)

**Settings → Data & Maps → CHECK FOR UPDATES**, then INSTALL a pack.

The published catalog currently carries two data packs:

| Pack | Runtime file on SD | Runtime size |
| --- | --- | --- |
| FAA AIRCRAFT | `/orcsdr/data/adsb_aircraft.idx` | 34 MB |
| FAA AVIATION | `/orcsdr/data/faa_aviation.idx` | 3 MB |

`NOAA WEATHER` and `FCC FM / AM` show **NOT PUBLISHED** because the signed
catalog genuinely does not contain them yet — the ids are reserved, but no
artifacts exist. Their INSTALL button reads UNAVAILABLE and does nothing;
that is accurate, not a bug. (You do not need the NOAA pack to *listen* to
weather radio — the seven NWR channels are built into the Weather screen.)

### Sideloading: direct download links

> **Reception stops while you download, and that is deliberate.** A streaming
> RTL-SDR and the Wi-Fi co-processor link both DMA out of PSRAM, and the
> contention is severe: measured on hardware, Wi-Fi association succeeded
> **0 out of 15** times while the dongle was streaming, and **15 out of 15**
> with the stream stopped (dongle still plugged in). The firmware now pauses
> reception automatically for catalog checks, downloads and Wi-Fi connects, and
> restores it afterwards. You do not need to unplug anything.

The 34 MB FAA aircraft download over the Tab5's Wi-Fi co-processor is still the
slowest path there is. **Copying the files to the SD card from a PC is faster
and cannot fail halfway.**

Download the file you want, then copy it to the SD card at the path shown. The
device only reads the `.idx` runtime files — the `.zip` archives are
provenance copies of the original sources and are never opened by the
firmware, so skip them unless you want the raw data for yourself.

| What you get | Download | Copy to SD card as | Size |
| --- | --- | --- | --- |
| Aircraft registration lookup (ADS-B) | [faa_aircraft-runtime.idx](https://github.com/hardcoreerik/OrcSDR/releases/download/data-catalog-v1/faa_aircraft-runtime.idx) | `/orcsdr/data/adsb_aircraft.idx` | 34.0 MB |
| Airport / ATC frequencies | [faa_aviation-runtime.idx](https://github.com/hardcoreerik/OrcSDR/releases/download/data-catalog-v1/faa_aviation-runtime.idx) | `/orcsdr/data/faa_aviation.idx` | 3.0 MB |

**Note the rename.** The aircraft pack downloads as `faa_aircraft-runtime.idx`
but the firmware looks for `adsb_aircraft.idx`; the aviation pack downloads as
`faa_aviation-runtime.idx` and the firmware looks for `faa_aviation.idx`. Get
the destination name wrong and the dashboard will keep saying NOT INSTALLED.

Steps:

1. Download the `.idx` file(s) above on a PC.
2. Put the Tab5's SD card in the PC. Create the folder `orcsdr\data` at the
   root of the card if it is not already there.
3. Copy each file in, renaming it to the destination name in the table.
4. Eject the card, put it back in the Tab5, and reboot. The ADS-B dashboard
   shows INSTALLED and Data & Maps agrees.

All eight assets, including the source archives and the signed manifest, are
on the
[data-catalog-v1 release page](https://github.com/hardcoreerik/OrcSDR/releases/tag/data-catalog-v1).

The Windows helper `tools/copy_to_tab5_sd.ps1` can do the copy if the card is
already mounted.

## 3. An offline map for your own area

Maps are OrcMaps PMTiles packs now. The firmware carries a built-in world
overview, and a regional pack covers the rest of the world at street-level
zoom, so there is nothing to build for your own area: install the regional
pack from **Settings → Data & Maps**, or copy it onto the SD card by hand.
**`docs/user-guide/offline-maps.md`** has both routes, the file names and
the checksum step.

The older single-file format (`lane_county_map.idx`, and the `local_map.idx`
that this fork's `build_orcmap.py` used to make) is no longer read. A card
that still holds one shows it as **UNSUPPORTED FORMAT; FILE PRESERVED** in
the map list -- the file is left alone, and installing a PMTiles pack
replaces it.

## 4. ATC presets for your area

This build does **not** read the fork's `/orcsdr/data/local_atc.idx`. Airband
listening goes through the Airband dashboard and its own catalog, which
replaced the single "Listen to ATC" preset this fork used to fill from
OurAirports. Bringing the nearest-airfield lookup over to that catalog is an
open item in `docs/FORK_HANDOFF.md`.

## 5. Your local P25 system

P25 trunking is **not** preconfigured for any region — a fresh device reports
`No P25 system configured`, and it should, because control channels are
entirely local. The device stores as many named system profiles as you like on
the SD card and lets you switch between them.

Write a profile like this, using your system's data from
[RadioReference](https://www.radioreference.com/) or your own survey:

```ini
version = 2
system_name = King County EPSCA
nac = 755
wacn = 781824
system_id = 499
control_channel_hz = 851012500
control_channel_hz = 852537500
control_channel_hz = 853762500
auto_follow = true
encryption_skip = true
talkgroup = 1101, Seattle PD Dispatch
talkgroup = 1210, Seattle Fire Dispatch
```

Format notes — the parser is strict, and a bad line is rejected with its line
number rather than silently ignored:

- **Every number is decimal.** RadioReference prints NAC, WACN and System ID in
  hex (`0x2F3`, `0xBEE00`, `0x1F3`); convert them first. The three above are
  those same values in decimal.
- **Control channels are in Hz**, not MHz: 851.0125 MHz is `851012500`.
  Up to 8 of them, one `control_channel_hz` line each.
- **Talkgroups are `id, alias`** — the comma is required. Up to 8.
- `#` and `;` start a comment; blank lines are fine.
- `nac` ≤ 4095, `wacn` ≤ 1048575, alias ≤ 31 characters.

1. Save it to the SD card as `/orcsdr/p25-import.cfg`.
2. On the device: **P25 dashboard → SETUP tab → IMPORT**.
3. Select the imported profile to make it active.

Imported profiles live under `/orcsdr/p25/<id>/profile.cfg` and can be
exported, renamed and deleted from the same tab. The SURVEY button then sweeps
your control channels and picks whichever one is actually decoding, so you do
not have to know which of them your site is using.

Only publish or share talkgroup aliases you have the right to redistribute —
see `docs/DATA_SOURCE_LEDGER.md`.

## 6. What is still fixed

- **NOAA Weather** — the seven NWR channels (162.400–162.550 MHz) are the same
  everywhere in the US, so the Weather screen's channel picker needs no local
  data at all. Which transmitter you hear depends only on where you are.
- **CB** — the 40 channels are fixed by regulation.
- **Marine VHF** — 40 channels, fixed internationally. The set here is the US
  monitoring plan: every simplex working channel plus 16 (distress and
  calling), 13 (bridge-to-bridge), 09 (boater calling) and 22A (Coast Guard
  liaison and safety broadcasts). Receive-only, like everything else here.
- **NOAA SAME alerts** — nothing to configure. The weather band decodes the
  alert header automatically and reports the event code and the six-digit FIPS
  code of every county covered. Look yours up once (they are published by the
  Census Bureau and by the NWS) and you will know at a glance whether an alert
  is for you: Prince William County VA, for example, is 51153.
- **GMRS / FRS** — 30 channels, also fixed by regulation: 1–7 and 8–14 are the
  462/467.5625 MHz interstitials, 15–22 the 462.5500 MHz main channels, and
  R15–R22 the 467.5500 MHz repeater *inputs* (listening on those hears the
  station uplinking to the repeater rather than the repeater's output, which
  15–22 already carry). Receive-only: tuning lands on the channel raster
  (12.5 kHz filter on 8-14), which is not authority to transmit — GMRS needs
  an FCC licence.

**CB** has a channel scanner with lockout on the CB dashboard. **GMRS and
marine** have one on Home: tune into either band and the STEP SIZE box (which
only says FIXED there) becomes **SCAN**. It walks the channel list, stops on a
busy channel, and picks up again about 2.5 s after it falls quiet; tap it again
to stop, or tune by hand. Lock out a channel that is always busy with
`RTL_CHANNEL_LOCK <name>` (for example `RTL_CHANNEL_LOCK R15`).

The scanner's thresholds are **placeholders until you set them on air**. Tune
an idle channel and run `RTL_CHANNEL_PROBE` a few times to see the noise
reading, then a channel with someone talking; set `RTL_CHANNEL_MIN_SNR`
between the two and, if needed, `RTL_CHANNEL_SQUELCH`. Both are saved. The CB
squelch is separate: SQL+/SQL- on the CB panel, or `RTL_SQUELCH <dBFS>`.
> **Check your Meshtastic modem preset before blaming the receiver.**
> Meshtastic moved the US stock preset off **LongFast** (250 kHz, SF 11)
> because its bandwidth is not US-compliant -- FCC 15.247 expects at least
> 500 kHz for digital modulation. The 500 kHz presets are **Long Turbo**
> (SF 11) and **Short Turbo** (SF 7).
>
> The grids differ, so a slot number means nothing without the bandwidth:
> slot 14 is 905.375 MHz at 250 kHz but **908.750 MHz** at 500 kHz. Read the
> frequency your node displays, not the slot. "Frequency slot 0" means auto,
> where Meshtastic hashes the channel name to choose one.
>
> **This fork decodes 250 kHz presets only.** The native decoder is built
> around 250 kHz and refuses 500 kHz outright, so a node on the current US
> stock preset is invisible to it. `RTL_LORA_MODEM <sf> <bw_hz>` will tune and
> trigger on 500 kHz but cannot decode it yet. See `docs/FORK_HANDOFF.md` 3d.

- **LoRa** — the Meshtastic US band is 104 frequency slots of 250 kHz starting
  at 902.125 MHz, and **slot 20 (906.875 MHz) is the LongFast default**. The
  LoRa dashboard's CHANNELS button opens a picker for stepping slots or jumping
  to a quick slot, and the chosen slot is remembered across reboots — regional
  meshes often move off the default, e.g. **NoVA Mesh runs slot 9
  (904.125 MHz)** with LONG_FAST and hop limit 7. It is a receive-only monitor,
  not a participant in the mesh.

  If you see no traffic, check `RTL_LORA_NATIVE_STATUS` over serial:
  `preambles=0` means nothing was even detected (antenna or an idle mesh),
  whereas `crc_failures` climbing means bursts are arriving but not decoding.

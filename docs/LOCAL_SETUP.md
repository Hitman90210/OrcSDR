# Making OrcSDR local to you

Several OrcSDR features need data about *where you are*: the ADS-B radar map,
the "Listen to ATC" preset, and P25 trunking. Out of the box the device ships
with none of that, and the one published map pack covers Lane County, Oregon.
This is how to replace each piece with your own.

Everything here is done from a PC with the SD card, or over Wi-Fi. Nothing
needs a rebuild of the firmware.

## 1. Set your receiver location

**ADS-B dashboard → SETUP tab → latitude / longitude.**

This is the origin of the radar plot and the anchor for the nearest-ATC
lookup. Without it the radar has nothing to centre on and the ATC card reads
`NEEDS YOUR LOCATION`. Use decimal degrees, negative for west/south.

## 2. Data packs (FAA aircraft, FAA aviation)

**Settings → Data & Maps → CHECK FOR UPDATES**, then INSTALL a pack.

The published catalog currently carries three packs:

| Pack | Runtime file on SD | Runtime size |
| --- | --- | --- |
| FAA AIRCRAFT | `/orcsdr/data/adsb_aircraft.idx` | 34 MB |
| FAA AVIATION | `/orcsdr/data/faa_aviation.idx` | 3 MB |
| LANE COUNTY MAP | `/orcsdr/data/lane_county_map.idx` | 29 KB |

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
| Lane County, Oregon map | [lane_county_map-runtime.idx](https://github.com/hardcoreerik/OrcSDR/releases/download/data-catalog-v1/lane_county_map-runtime.idx) | `/orcsdr/data/lane_county_map.idx` | 29 KB |

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

`tools/build_orcmap.py` builds the device's map format for any bounding box
from OpenStreetMap, so you are not stuck with Lane County:

```bash
python apps/orcsdr-tab5/tools/build_orcmap.py \
  --bbox 47.50 -122.45 47.75 -122.20 --out local_map.idx
```

Arguments are `SOUTH WEST NORTH EAST` in decimal degrees. Copy the result to
the SD card as **`/orcsdr/data/local_map.idx`** — the firmware loads that in
preference to any packaged map.

The device holds 640 line segments and 32 labels so the map fits in RAM beside
the DSP, which is roughly one metro area at useful detail. The builder reports
what it kept:

```
Wrote local_map.idx: 640/640 segments (437 road, 200 water, 3 airport), 4/32 labels.
```

If it says the budget was reached, either shrink `--bbox` or raise
`--tolerance` (degrees, default `0.002` ≈ 200 m) so the coverage spreads
evenly across the whole box instead of running out in one corner.

Data © OpenStreetMap contributors, ODbL.

## 4. Your local P25 system

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

## 5. What is still fixed

- **NOAA Weather** — the seven NWR channels (162.400–162.550 MHz) are the same
  everywhere in the US, so the Weather screen's channel picker needs no local
  data at all. Which transmitter you hear depends only on where you are.
- **CB** — the 40 channels are fixed by regulation.
- **LoRa** — defaults to the Meshtastic US LongFast slot (906.875 MHz). Change
  the frequency on the LoRa dashboard for other regions; it is a receive-only
  monitor for Meshtastic mesh traffic, not a participant in the mesh.

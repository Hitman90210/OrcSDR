# Maps and data packs

OrcSDR receives perfectly well with an empty SD card. Everything here is
optional and adds *names* to what the radio already hears: which airline that
ADS-B contact belongs to, which airport a frequency serves, and what the ground
under a target looks like.

| You want | You need | Requires |
| --- | --- | --- |
| Aircraft registration, type and operator on ADS-B | `faa_aircraft` pack | Wi-Fi + SD card |
| Airport and ATC frequency lookup | `faa_aviation` pack | Wi-Fi + SD card |
| NOAA transmitter / SAME area names | `noaa_weather` pack | Wi-Fi + SD card |
| FM/AM station identification | `fcc_broadcast` pack | Wi-Fi + SD card |
| Roads, water and airports under the ADS-B radar | **a map you build yourself** | SD card + a PC, once |

---

## 1. Insert an SD card

Any ordinary card works. OrcSDR creates what it needs under `/orcsdr/`. Nothing
below works without one, because none of this fits in flash beside the firmware.

## 2. Data packs — Settings → Data & Maps → Check for Updates

Connect Wi-Fi first (**Settings → Connectivity**), then open **Settings → Data &
Maps** and choose **Check for Updates**. Pick the packs you want and let them
install.

What actually happens, since it is worth knowing what your device is talking to:

- The Tab5 fetches one signed `catalog-v1.json` **only when you press that
  button**. It never sends your Wi-Fi credentials, your receiver coordinates,
  your labels, or anything you have received.
- The manifest is signed with a P-256 key whose public half is compiled into the
  firmware, and the asset URLs are covered by the signature, so a tampered
  GitHub release cannot redirect a download.
- Each pack streams to a `.part` file, is SHA-256 and format checked, and is
  only then swapped in, keeping a `.bak` of the previous copy. **A failed or
  interrupted update leaves the working pack in place.**

Packs land in `/orcsdr/data/`. Sizes are tens of megabytes, so give the download
a few minutes on a slow link.

> The catalog is published from the upstream project's release, and this fork
> uses upstream's signing key unchanged, so the same packs work here.

## 3. Maps — build one for your own area

**Read this part first: there is no map pack for you to download.** The only map
in the catalog covers Lane County, Oregon, which is where the upstream author
lives. For everyone else it is scenery from somewhere they have never been.

So OrcSDR loads a map you build instead, and prefers it over the packaged one.

### Build it

You need Python 3 on a PC and an internet connection **once**. No account, no
API key, nothing to install:

```bash
python apps/orcsdr-tab5/tools/build_orcmap.py --center 44.05 -123.09 --range-nm 25 --out local_map.idx
```

Replace the latitude and longitude with your receiver's. `--range-nm` should
roughly match the ADS-B radar range you use (default 25). If you would rather
give an explicit box, `--bbox SOUTH WEST NORTH EAST` does that instead.

It queries [OpenStreetMap](https://www.openstreetmap.org/) via Overpass for
major roads, named rivers and lakes, coastline, runways and place names, then
fits them into the 640 segments and 32 labels the firmware holds. You will see
something like:

```
Wrote local_map.idx: 640/640 segments (426 road, 200 water, 14 airport), 7/32 labels.
Segment budget was reached -- raise --tolerance, or shrink --range-nm/--bbox,
for a map that covers the whole area evenly.
```

Hitting the budget is normal and not an error: the tool gives each class its own
share and takes the longest ways first, so motorways and rivers survive and
minor detail is what gets dropped.

### Install it

Copy the file to the SD card as exactly:

```
/orcsdr/data/local_map.idx
```

Reboot. Open **ADS-B**; the radar now draws your area, and the panel says
**LOCAL MAP** rather than naming the packaged sample.

### If Overpass is busy

Public Overpass endpoints rate-limit and time out under load. The tool retries a
lighter query automatically. If both fail:

- wait a few minutes and run it again, or
- reduce `--range-nm`, or
- run the Overpass query yourself in a browser at
  [overpass-turbo.eu](https://overpass-turbo.eu/), save the JSON, and feed it in
  offline:

```bash
python apps/orcsdr-tab5/tools/build_orcmap.py --center 44.05 -123.09 --geojson saved.json --out local_map.idx
```

### The file format, if you would rather generate it another way

Plain ASCII, LF line endings, first line exactly `ORCMAP1`:

```
ORCMAP1
R 44.05929 -123.10120 44.05508 -123.10144
W 44.06238 -123.04993 44.06233 -123.04876
A 44.12290 -123.21200 44.12690 -123.21160
L 44.05051 -123.09505 Eugene
```

`R` road, `W` water, `A` airport, each followed by two lat/lon pairs. `L` is a
label: lat, lon, then up to 23 characters of text. At most **640** segments and
**32** labels; the firmware stops reading past that, and lines must stay under
96 characters.

---

## Where everything lives

```
/orcsdr/
  data/
    adsb_aircraft.idx     aircraft registry        (faa_aircraft pack)
    faa_aviation.idx      airports and ATC         (faa_aviation pack)
    local_map.idx         YOUR map                 (you build this)
    lane_county_map.idx   packaged sample map      (catalog, optional)
    local_atc.idx         nearest-airport cache    (derived on device)
  p25/                    P25 system profiles      (catalog, optional)
  exports/                CSV exports you trigger
  screenshots/            screen captures
  recordings/             audio and IQ captures
```

## Troubleshooting

**"Check for Updates" does nothing.** Wi-Fi must be connected and an SD card
present. Settings → Connectivity shows the link state.

**A pack shows NOT INSTALLED after a reboot even though the file is there.**
Fixed — the check now asks the filesystem rather than a manifest that resets at
boot. If you see it on older firmware, run Check for Updates once more.

**The radar shows range rings but no map.** That is the normal, working state
with no map file. Aircraft are plotted by range and bearing and do not need one.
Build `local_map.idx` if you want the ground drawn too.

**The map looks sparse.** The firmware holds 640 segments. Lower `--range-nm`
to spend them over a smaller area in more detail.

## Privacy

Your receiver location is used on the device to turn aircraft positions into
range and bearing. It is not sent to the catalog, and the LAN companion console
is deliberately given range and bearing rather than coordinates so it cannot
disclose where you are. See [safety and privacy](safety-privacy.md).

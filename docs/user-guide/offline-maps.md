# Offline maps

**Current status: production map catalog; firmware release candidate under validation.**
Current configured builds use the signed, non-expiring production catalog and
GitHub-hosted global map pack. Older OrcSDR releases may not include these
controls. Final firmware acceptance and release packaging are tracked separately
from the already activated map catalog.

One installed map pack serves the location picker, LoRa, ADS-B and FT8 maps.
Each view chooses its own area and overlays; you do not download a separate
world file for each dashboard. After installation, the map works offline.
Set your receiver location so receiver-centered views know where to start.

The current global regional pack covers zoom levels 0–8. Its archive is
543,380,195 bytes: about 543 MB, or 518 MiB. The app may show the binary-unit
size as about 518 MB. Where installed, the smaller firmware world overview
remains available without an SD card.

## Download directly on the Tab5

1. Insert a working SD card with enough free space and connect OrcSDR to Wi-Fi.
2. During setup, open **Offline Maps**. To return later, open **Settings →
   Data & Maps → LOCAL MAPS**.
3. Press **GET MAPS**, then **CHECK CATALOG**.
4. Select **World regional detail - zoom 0-8** and press **DOWNLOAD/RESUME**.
5. Wait for download and installation to finish. The new quick-check path checks
   the saved file's size, PMTiles header, root directory and map coverage. It does
   not reread the whole map or verify its payload checksum. The signed catalog
   and HTTPS checks remain. Older firmware performed full-file scans; its
   [PNY-card timing record](https://github.com/hardcoreerik/OrcSDR/blob/main/docs/evidence/maps/download-benchmarks/2026-10-24-com17-pny32/README.md)
   does not predict the new installation time. Damaged tile data beyond the
   quick check can remain undetected until it is used.
6. Press **INSTALLED**, select the new map, then **USE THIS MAP**.

The device downloads from GitHub release assets in the
[OrcMaps repository](https://github.com/hardcoreerik/OrcMaps/releases).
Current configured builds use the
[production signed catalog](https://github.com/hardcoreerik/OrcMaps/releases/tag/maps-catalog-v1).
It has no scheduled expiry. Earlier pilot builds used a separate test catalog.

**STOP TRANSFER** and **DOWNLOAD/RESUME** support interrupted transfers.
**RETRY LAST** is offered for recoverable failures. Existing installed maps
remain available while a replacement is being prepared. If the app says
**Map source is not configured**, that firmware has no configured remote
catalog; manual installation remains an option on builds supporting OrcMaps.

To pause a download, press **STOP TRANSFER** and wait for it to stop. Keep that
SD card and its partial-download files. Reconnect to Wi-Fi when needed, then
select the same pack and press **DOWNLOAD/RESUME** (or **RETRY LAST** after an
error). The device resumes from its last saved checkpoint; a small amount of
unsaved progress may need downloading again. It checks that the remote file
still matches before appending, then performs the quick file check described
above. An interrupted download is not an installed map until installation finishes.

## Copy the map onto the SD card using a computer

1. Open the [global-map download page](https://github.com/hardcoreerik/OrcMaps/releases/tag/sha256/16adde0b4f67734535db6871b89949e68c7e1dabf1c34d2bd7bd23842a9e5991).
   Under **Assets**, download:
   - `global-regional-z0-z8.pmtiles` — the large map archive.
   - `global-regional-z0-z8.manifest.json` — the map description used by OrcSDR.
   - `global-regional-z0-z8.sha256` — the checksum for checking the download.
2. Shut down the Tab5 before removing its card. On the computer, open the
   card and create a folder called **orcmaps** at its root if it is missing.
   Keep existing maps and user files.
3. Copy the three files directly into that folder, keeping their names:

   ```text
   SD card/
     orcmaps/
       global-regional-z0-z8.pmtiles
       global-regional-z0-z8.manifest.json
       global-regional-z0-z8.sha256
   ```

4. Safely eject the card, put it back in the Tab5 and start OrcSDR.
5. Open **Settings → Data & Maps → LOCAL MAPS**, press **RESCAN**, select
   **World regional detail - zoom 0-8**, then **USE THIS MAP**. The setup
   wizard's Offline Maps page can select the same installed pack.

The `.pmtiles` and `.manifest.json` files must have matching base names.
OrcSDR discovers these pairs directly in `/orcmaps/`; the current integration
does not use a nested `/orcmaps/packs/<name>/` layout. The checksum sidecar is
useful for verification on the computer. Manually copied packs currently get
metadata/size checks during discovery; that scan does not perform a full archive
hash check. For manual copying, verify the checksum on the computer.

For this exact global regional archive, the expected SHA-256 is:

```text
16adde0b4f67734535db6871b89949e68c7e1dabf1c34d2bd7bd23842a9e5991
```

On Windows, replace `E:` with the card's actual drive letter:

```powershell
Get-FileHash E:\orcmaps\global-regional-z0-z8.pmtiles -Algorithm SHA256
```

Compare the result with the downloaded `.sha256` file. Letter case does not
matter. No account, firmware flash or catalog-signing key is needed for copying
these published files onto an already compatible device.

## If the map is missing or unavailable

- Confirm that both matching files are directly inside the card's `orcmaps`
  folder and that the archive download completed. Press **RESCAN** afterward.
- Check the complete filenames; a manifest saved with an extra `.txt` extension
  will not be discovered.
- Use the archive and manifest from the same release. Do not edit a manifest
  to bypass an unavailable/incompatible-pack message.
- Older `ORCMAP1` `.idx` data is a different format. Keep existing user files;
  renaming them does not turn them into a PMTiles pack.

## What the app explains today

The **GET MAPS** page explains catalog checking, connection/card requirements,
download/resume and installed-map selection. After installation it tells you to
open **INSTALLED** and use **USE THIS MAP**.

A prepared firmware change adds the manual-copy filenames/folder and a plain-text
release-page address to the Local Maps footer when no transfer/error notice needs
that space. The same information is available through `RTL_MAP HELP`. Its focused
host and P4 syntax checks pass; it is not yet installed on COM17 at this checkpoint.

The full manual guide remains this document. The app does not yet offer a
clickable download link/QR code or a ready-to-copy bundle.

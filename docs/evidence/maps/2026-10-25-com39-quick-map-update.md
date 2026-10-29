# COM39 app-only update after completed map download

User authorized the same updated build on COM39 after its download completed.
Target verified as COM39, USB serial `E8:F6:0A:E2:D6:03`. No COM17 access occurred.

Before flashing, read-only status showed phase complete, busy=0, 543,380,195
downloaded bytes and `global-regional installed=1`. The old firmware reported
two full verification passes. This is completion evidence, not an install-time
measurement: this session did not capture the download's start time.

Installed app source `9e3f0378`, identical to COM17's verified candidate:
SHA-256 `4a3639853609778b711c2168bc418b8637e15a7b4f95e8eb3433c58bc6339ed3`,
5,301,584 bytes. Includes quick file checks, clock tap shortcut, Back wording,
automatic catalog-time handling and the earlier map memory corrections.

## Actual result

- Previous app `6f625f66`, bootloader and partition table verified first.
- Fresh NVS/PHY/coredump/world backups saved and verified against flash.
- Only the P4 app at 0x10000 written. App and all preserved regions subsequently
  verified. No erase, C6 update or SD pack modification by the flashing tools.
- Startup reached Ready with SD and receiver available; Home self-check passed.
- After boot, C6 still **1.4.1**, transport ready; optional embedded 3.0.6 image
  was not applied.
- Receiver location remains configured. Map inventory reports embedded world
  and downloaded **World regional detail - zoom 0-8**, both usable.
- The downloaded map's discovery status is truthfully SIZE CHECKED; HASH
  UNCHECKED. Inventory discovery does not rescan the archive payload.
- Device left on Home; serial sessions closed. Wi-Fi was off on this boot.

After reboot the in-memory downloadable catalog list is empty until checked
again; that is separate from the usable installed-pack inventory above.
No fresh download, quick-check timing benchmark, physical tap acceptance or
unset-clock SNTP test was performed on COM39. The old completed download still
used its prior full-scan policy; later downloads use this new quick-check app.

## Commands and raw evidence

Run directory `F:\AI\OrcSDR-TEMP\maps-integration-completion`; raw evidence stays
there outside Git:

- `python .local/maps/com39_bench.py .local/maps/com39-quick-map-preflight-plan-20261025.json com39-quick-map-preflight-device-20261025.log` — completed; transfer finished before reset.
- `python .local/maps/flash_com39_quick_map_20261025.py` — PASS;
  `com39-quick-map-{verify-previous,verify-backups,write,preservation}-20261025.log`.
- Frozen artifacts and hash manifest:
  `.local/maps/com39-quick-map-9e3f0378-20261025/manifest.json`.
- `com39-quick-map-boot-native-monitor-20261025.log` — startup capture, closed.
- `python .local/maps/com39_bench.py .local/maps/com39-quick-map-postflash-plan-20261025.json com39-quick-map-postflash-device-20261025.log` — completed; C6/location/installed inventory checks above.

Next milestone: normal user navigation/map use on the updated COM39; retain
separate outstanding release review, timing and hardware-matrix gates.

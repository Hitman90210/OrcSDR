# Receiver Location text correction — COM17

User approved build and COM17 app-only update for 984f0b5c plus preserved wizard
edits. Native build passed; app size 0x4d1420 (5,051,424 bytes), 20% partition
headroom. Pin, miniz and FT8 placement gates pass. Build log:
`apps/orcsdr-tab5/location-text-native-build-20261018.log`.

Target identity checked before each esptool operation: COM17,
30:ED:A0:E2:E6:95. Previous app verified against the saved label candidate.
Current NVS was separately backed up and verified to preserve the newly
approved serial pairing. App-only write and preservation verification passed
for bootloader/table/NVS/PHY/coredump/world. C6 unchanged, still 3.0.6.
Artifacts and manifest: `.local/maps/location-text-984f0b5c-20261018/`.
Logs: `location-text-flash-run-20261018.log` and individual operation logs.

Boot capture passed (`location-text-boot-20261018.log`). Authenticated test
opened Settings/Receiver Location, entered map picker, pressed Use Location,
and verified screen=settings. Saved screenshot transferred with hash checking
after pausing reception for the expected bulk-transfer radio_busy guard.
Logs: `location-text-check-20261018.log`, `location-text-capture-20261018.log`.

Inspected `.local/maps/location-text-after-20261018.bmp` at native resolution:
the selected Oregon pack identifier ends in ellipsis and remains within its
row; no text crosses the left rail. Returned coordinates are visible. This
is captured-image acceptance, not the user's physical-panel confirmation.

Reception restored via Shortwave dashboard and RTL_FREQ 14070000, then Settings
Receiver Location reopened. Log: `location-text-restore-20261018.log`.
COM39 untouched. No SD packs overwritten and no C6 updates performed.
The broader map performance, font coverage and delivery/setup plan remains open.

# COM17 fresh-settings first-boot walkthrough

User explicitly requested true first boot and manual progression through each screen.
COM17 USB/ROM identity 30:ED:A0:E2:E6:95 checked for each esptool operation.
Current P4 app remains source 329e5a9e; no build or app flash performed.

Private recovery: `.local/maps/com17-recovery/first-boot-20261018-100824/`.
NVS backup is 24,576 bytes; verify_flash passed before erasure. Manifest records
SHA256. Partition table readback confirmed nvs at 0x9000, size 0x6000.
Only that region was erased; readback verified all bytes 0xff.
C6, app, embedded world map and SD files were not written by this procedure.
Existing SD contents mean this is a fresh-settings test, not an empty-SD test.
Saved Wi-Fi/location/preferences/Tab5 pairing state were cleared as authorized;
OrcDial itself was not reset or erased. Backup permits deliberate later restoration.

Commands: `python .local/maps/com17_first_boot.py`, then
`python .local/maps/capture_com17_boot.py com17-first-boot-startup-20261018-100824.log`.
Backup/verify/table/erase/readback logs share prefix `com17-first-boot-` and suffix
`20261018-100824.log`, saved at the checkout root where scripts ran.
Startup reached BOOT_STAGE ready; summary sd=1 wifi=0 rtl=1.
No setup buttons were automated. Physical screen acceptance and each setup step
remain for the user's walkthrough: Welcome, Network, Location, Offline Maps, Complete.

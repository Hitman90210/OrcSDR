# Reconciled native candidate on COM17

Source `137e244a2912b03bca79c646ab0aeb96a71316ee` includes local merge `97f48640`
through fetched main `fb70e969`. Engine pin remains published `2133547`;
unpublished offscreen optimization is excluded.

## Actual build and inspection

From the integration root:

```text
python .local/maps/build_reconciled_20261023.py build
C:/Espressif/python_env/idf5.5_py3.14_env/Scripts/python.exe .local/maps/inspect_reconciled_20261023.py
```

Both exited 0. Explicit saved SDKCONFIG, C6 3.0.6 input, diagnostics OFF and
TEST-ONLY catalog/public key pass inspection. App 5,284,496 bytes; partition
headroom 1,006,960 bytes. DMA reserve 40 KiB; exceptions reserve 1 KiB.
Miniz/FT8 PCM symbol gates pass; map replay/labels/preview/catalog state and
FT8 previous snapshot remain in PSRAM. Manifest records measured symbol sizes.
App SHA-256 `ac5267a55ce1c2172b8acff77be555232d7b18efbc3145e8df5c64964c5eb040`.
ELF SHA-256 `07d674c65663d5cd5b8506cfa10da1a80bd216ff570acf499704447fea6c3216`.
Artifact/backups: `.local/maps/maps-reconciled-137e244a-20261023/`.

## Preservation and startup

COM17 USB serial `30:ED:A0:E2:E6:95` verified before each esptool operation.
Preflight found active FT8 at 14.074 MHz, zero driver drops/overruns; the old
LoRa setup was not assumed current. Authenticated RTL_STOP succeeded.
`flash_reconciled_20261023.py` verified the installed f38b8601 app against its
saved image, then saved and verified fresh NVS, PHY, coredump and world backups.
Only P4 app at 0x10000 was written. New app and prior boot/table/non-app data
verified afterward. No C6 update or SD file write; COM39 untouched. Verified
f38b8601 remains available for rollback.

Native esp_idf_monitor captured post-flash startup with the matching saved ELF.
Device ELF prefix 07d674c65 matches; SD/Wi-Fi/receiver reached Ready. Initial
Wi-Fi retry recovered before Ready. Monitor closed cleanly. No decoder testing.
Follow-up named commands entered setup with reception stopped. Location remains
E7 440628866,-1230160273. At uptime 112,776 ms: free PSRAM 21,960,740 bytes,
largest PSRAM 21,495,808, largest DMA 20,480, main stack high-water mark 4,652.
These are setup-idle figures, not loaded map budgets. Catalog check succeeded:
one product, existing global pack installed. Physical map acceptance and new
loaded timing are not proven.

## Logs and next test

Logs saved in invocation directory:

- maps-reconciled-native-build-20261023.log
- maps-reconciled-native-inspection-20261023.log
- maps-reconciled-preflash-device-20261023.log
- maps-reconciled-stop-device-20261023.log
- maps-reconciled-verify-previous-20261023.log
- maps-reconciled-backup-*-20261023.log
- maps-reconciled-verify-backups-20261023.log
- maps-reconciled-app-write-20261023.log
- maps-reconciled-preservation-20261023.log
- maps-reconciled-boot-native-monitor-20261023.log
- maps-clean-github-preparation-device-20261023.log

User requested a clean GitHub-to-device install with timings. It has NOT started:
the current pack triggers installed-file reuse. Serial removal deliberately
excludes /orcmaps/. A spare card, user-preserved files or a safe reinstall path
is needed. Physical preparation question is pending. No working pack deleted.
Prepared five-second observer is not a running download. Record download,
verification/activation and total separately; then verify reboot/offline rendering.

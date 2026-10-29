# Production catalog activation

User explicitly approved production key creation/storage, signed publication,
map metadata promotion, tested builder push and production-configured COM17
validation. No main merge, firmware tag, erase, C6 update or COM39 access.

## Published

- [Production catalog](https://github.com/hardcoreerik/OrcMaps/releases/tag/maps-catalog-v1)
  is a published release, not TEST-ONLY. One global-regional product;
  `expires_at: null`, revision **20261025005000** (UTC generation; October 24 local).
- [World regional map](https://github.com/hardcoreerik/OrcMaps/releases/tag/sha256/16adde0b4f67734535db6871b89949e68c7e1dabf1c34d2bd7bd23842a9e5991)
  is promoted out of prerelease/TEST-ONLY metadata. The original 543,380,195-byte
  archive asset metadata is unchanged. The production manifest was added under
  `global-regional.production.manifest.json`; historical pilot assets remain.
- Builder commits through **5594d2adeed72aa13fae7e10b18f53169690394e** pushed to
  the existing development branch after 13 focused tests passed.

Catalog SHA-256: `126c46414b9a6acade3b328efab85cdd52ac692e88a902db9b8dd5693b25106c`.
Public PEM SHA-256: `d06ee971018586562dbe21cf44030a1d9e13d7b0fe65b6d54d3d7a4a4c7562dc`.
Signature asset SHA-256: `9bd641c55a6623577dabd333c8532d951e352a87e814b62cc0fdc19173705bf6`.
Direct public HTTPS reads match these signed local artifacts byte for byte.

## Key custody

Separate P-256 production private key is outside both repositories in the approved
user-private directory. ACL inheritance is removed; current user and SYSTEM have
access. A protected local backup copy exists. **Off-machine backup is not yet
established**: the local copy protects against file loss, not machine/disk loss.
No private key bytes appear in logs, Git, firmware or release assets. Public key
is committed as `apps/orcsdr-tab5/main/maps_production_public.pem`.

## Source and validation

`build-tab5-idf.ps1` now explicitly supplies the production URL/public key, including
when invoked by release packaging. Explicit overrides remain available; low-level
manual CMake builds without catalog inputs remain offline/unconfigured. This
prevents a stale pilot cache from silently becoming the standard script's input.
No M5Burner release package has been built or accepted in this step.

The signing script uses the tested OrcMaps assembler and actual local archive;
strict pack preflight and full archive hash validation pass. No archive transfer
or duplication of archive bytes was required (local hard link used). Signature
verifies with the production public key. Actual signed production payload passes
OrcSDR's host parser/session using the persisted pilot revision/hash; no trust-floor
reset. Test harness accepts an explicit expected revision instead of only fixture 1.
Six catalog configuration checks and PowerShell parser check pass.

Logs beside invocation in OrcSDR integration root:
`maps-production-signing-20261024.log`, `maps-production-publication-20261024.log`,
`maps-production-data-promotion-20261024.log`, `maps-production-public-https-20261024.log`,
`maps-production-signed-consumer-{build-,}20261024.log`,
`maps-production-catalog-config-20261024.log`.
Public-only reports and preflight output: `.local/maps/production-20261024/`.
OrcMaps logs: `maps-production-builder-activation-20261024.log`,
`maps-production-builder-push-20261024.log`.

Commands are retained in `.local/maps/prepare_production_catalog_20261024.py`,
`publish_production_catalog_20261024.py`, `promote_production_map_20261024.py`.
They fail rather than overwrite an existing key or staging directory.

## COM17 production candidate

Source **46cf8148**, engine **6a621a7**. Native build and inspection pass:
`python .local/maps/build_production_maps_20261024.py build` and
`python .local/maps/inspect_production_maps_20261024.py`. Explicit saved SDKCONFIG,
diagnostics OFF, production URL/key and build ID; all normal symbol/placement
gates pass, internal DMA reserve remains 40960 bytes. App 5,289,664 bytes,
headroom 1,001,792 bytes. App SHA-256
`0deac20f0d034ca4c36cc91b93551b27e9917be8a4807a32fe2cc5637bef88a5`;
ELF `c034b3d8b165086998232b5e1c85f629e96066f23ea671cf05a952d93f082e96`.

`flash_production_maps_20261024.py` verifies prior `77a8cdce`, saves fresh
non-app backups, writes only P4 app and verifies app/non-app preservation. Native
monitor matches ELF prefix and reports SD/Wi-Fi/RTL Ready. No settings/trust
erase, world or C6 update. Recovery manifest and backups:
`.local/maps/maps-production-46cf8148-20261024/`.

Authenticated device plan:

```text
python .local/maps/com17_label_bench.py .local/maps/production-native-check-plan-20261024.json maps-production-native-check-device-20261024.log
```

Pass: `RTL_MAP DOWNLOAD CHECK` reaches `phase=6 busy=0 configured=1 ready=1
count=1`. Same receiver coordinates; installed regional and firmware overview
remain usable. Production manifest version differs from pilot, so catalog entry
correctly reports `installed=0`; no archive installation was requested. Reusing
identical archive bytes across changed manifests is not implemented by this step;
a future upgrade can require a download despite the old map remaining usable.
Do not call this an installed production-pack upgrade.

Home, graphics=1, LoRa streaming 906.875 MHz / 959,818 samples/s, 0 drops/overruns.
PSRAM free 3,190,180 / largest 3,145,728 bytes; largest DMA 22,528 bytes. Existing
automatic IQ activity is present; this is continuity evidence, not decoder testing.
Actual public signed catalog also passes the host parser at January 1, 2100,
with null expiry and the pilot revision floor retained.

Logs beside invocation: `maps-production-native-build-20261024.log`,
`maps-production-inspection-20261024.log`, `maps-production-flash-console-20261024.log`
and per-part flash logs, `maps-production-boot-native-monitor-20261024.log`,
`maps-production-native-check-device-20261024.log`,
`maps-production-native-validation-20261024.log`,
`maps-production-nonexpiry-future-20261024.log`.

Reboot and repeat catalog Check also pass with the same saved location and no
NVS erase: `maps-production-reboot-native-monitor-20261024.log` and
`maps-production-reboot-check-device-20261024.log`. Same signed catalog reaches
ready again after the previously accepted state is reloaded. Final Home reception
959,830 samples/s, zero drops/overruns; serial port released. No archive download
was requested in either check. Final firmware/M5Burner package
and firmware tag remain open. Installed maps remain available offline independently
of catalog expiry; no performance or full-goal completion claimed.

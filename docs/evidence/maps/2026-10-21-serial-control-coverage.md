# Map and setup serial-control coverage

User requirement, October 21: everything available to an OrcSDR user must have
a serial-command equivalent. This is a bounded source inventory of maps/setup,
not a completed application-wide audit or a claim that every route was tested.

| User operation | Current serial route | Remaining gate |
| --- | --- | --- |
| Inspect map availability, labels, rendering and installer state | `RTL_MAP_STATUS` | Source-camera diagnostic validated on COM17 with `b326a123`; full global-pack device gate is separate. |
| Open a dashboard | `RTL_UI OPEN ADSB`, `LORA`, `FT8`, `SETTINGS`, etc. | Existing authenticated route; check the actual screen owner, not acknowledgement alone. |
| Tap a visible control / enter a key | `RTL_UI TAP x y`, `RTL_KEYBOARD KEY ...` | Useful for UI-route regression; does not replace discoverable semantic map commands. |
| Set receiver lat/lon | `RTL_ADSB_LOCATION <latitude> <longitude>` | `7b2b1228` requires authentication and shares verified wizard/picker persistence. COM17 rejection, changed UI save and original-point restore pass; this legacy command is an explicit save. |
| Resolve approximate network location | `RTL_LOCATION IP`, then `RTL_LOCATION CONFIRM` | Existing lookup/confirm route; persistence and failure gates remain scoped. |
| Resolve an address or ZIP | `RTL_LOCATION LOOKUP <query>`, then `RTL_LOCATION CONFIRM` | Existing lookup/confirm route. |
| Inspect lookup state | `RTL_LOCATION STATUS` | Reports lookup result; add a clear saved receiver/map-camera status if not already exposed elsewhere. |
| Open setup / inventory / location picker | `RTL_MAP OPEN SETUP\|LIST\|PICKER` | Shared screen lifecycle, including reception pause; bounded COM17 ownership checks pass. Full setup interruption/recovery remains open. |
| Picker pan, zoom, proposed-coordinate entry, Save/Cancel | `RTL_MAP PICKER PAN`, `ZOOM`, `SET`, `SAVE`; `RTL_MAP CANCEL` | COM17 proposal/cancel and same-point save pass. Status distinguishes proposal from saved E7 coordinates. |
| Setup Back, Next, Skip, Quick Start, Finish | `RTL_MAP SETUP BACK\|NEXT\|SKIP\|QUICK\|FINISH` | Focused host coverage; COM17 traversed all five steps with optional pages skipped. All-method setup acceptance remains open. |
| Per-consumer Labels ON/OFF | `RTL_MAP LABELS PICKER\|LORA\|ADSB\|FT8 ON\|OFF` | Shared preferences/invalidation; f38b8601 COM17 four independent updates, controlled-reboot persistence, exact original restoration and saved-location retention PASS. [Native evidence](2026-10-23-independent-label-persistence.md). Physical button/glyph checks across all consumers remain separate. |
| Select/rescan a local OrcMaps pack | `RTL_MAP SELECT <full-id>`; `RTL_MAP RESCAN` | Unsupported/unknown selection rejection passes on COM17. `ba0e7a92` rescan refresh advances displayed frame revision 2→4. Selection, ownership persistence, reboot discovery and reselection pass in the approved October 22 TEST-ONLY pilot. |
| Check/download/retry/cancel an OrcMaps catalog pack | `RTL_MAP DOWNLOAD CHECK\|INSTALL <index>\|RETRY\|CANCEL` | Approved October 22 TEST-ONLY pilot passes actual 543 MB HTTPS install, cancel/retry, activation, reboot and offline frames. Production configuration and hardware failure-recovery matrix remain open. |
| ADS-B map range | Existing `RTL_UI ACTION SETTINGS RANGE <nm>`; dedicated `RTL_ADSB_RANGE [nm]` in `19856eea` | Installed with `050442b0`: COM17 named/Settings/routed-touch range checks pass, including rejection and persistence. Broader aircraft/RF acceptance stays open. |
| LoRa follow/centering and pan/zoom | `RTL_LORA_MAP STATUS`, `CENTER RECEIVER\|NODE`, `FOLLOW ON\|OFF`, `PAN east south`, `ZOOM IN\|OUT`; existing toggle route remains | `050442b0` is installed; bounded COM17 pan/zoom/receiver-center and streaming checks pass. October 22 positioned-node center/Follow and controlled NODEINFO/text checks pass within bounds. Follow identity while history reorders and final physical/performance acceptance remain open. |

The older `RTL_CATALOG_*` commands control generic signed data packs, not the
OrcMaps download catalog. Do not claim these already provide map-install control.
The retired regional ORCMAP1 entry is absent from their public listing and cannot
be installed; existing SD data remains preserved.

The dispatcher landed on the development branch in `1cd9b180`, with focused
optimized and sanitizer checks passing. See [command reference](../../MAP_SERIAL_COMMANDS.md)
and [device evidence](2026-10-21-map-serial-controls.md). This closes the named
map/setup dispatcher implementation gap, not application-wide parity or all
hardware acceptance. ADS-B range device acceptance and remaining LoRa
interactions are separate gates. October 22 checkpoints supersede the earlier
pending-port status: [dashboard/stack evidence](2026-10-22-map-dashboard-hardware-checkpoint.md)
and [coordinate confirmation/serial-save evidence](2026-10-22-setup-coordinate-confirmation.md).

October 23 reconciliation: these row updates use the later device evidence; they
do not claim app-wide serial parity. Current gates are in the
[completion matrix](2026-10-23-completion-acceptance.md).

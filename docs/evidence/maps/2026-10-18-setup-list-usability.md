# Setup and local-map usability candidate

User-approved scope: larger setup text, scrollable local-map list, understandable
map descriptions/details, location-method choices, easier Use Location, removal
of ADS-B radar grid and opposite fresh-install landscape orientation.

Prepared source changes:
- map_screen.cpp: four visible selectable map rows with swipe and Up/Down scrolling,
  readable title/coverage/zoom/status, separate Details, existing selection hook.
- Setup location choices reuse existing Settings network/address/coordinate entry
  through a small action hook; map selection remains offline-capable. Saved location
  appears on return. Internet lookup behavior and providers are unchanged.
- Use Location has a 60-pixel-high matching draw/hit rectangle, accepts the center
  without a prior pan, and reports save failures at text size 2. Back from setup's
  picker returns to method selection. No evidence yet of physical coordinate offsets.
- ADS-B removes four rings and horizontal/vertical radar lines. Aircraft and cardinal
  labels remain. Projection, range filtering and receiver operation are unchanged.
- Fresh orientation defaults change 1 to 3 at early startup, state load and Settings
  initialization; stored valid rotation values are preserved.

Tests: Linux focused CTest setup_flow, location_store, map_pipeline all PASS (3/3),
including actual picker/Settings code, scroll-to-fifth-map selection, current-center
save and existing changed-point/save-failure/cancel/pan/zoom tests.
Logs: setup-list-linux-build-check-20261018.log and
setup-list-linux-test-check-20261018.log. Earlier Windows ASan-host tests stalled;
only their own test/CTest processes were stopped. Initial Linux test exposed an
anonymous-type inline M5 stub problem; naming FakeM5 shares the display correctly.
Initial fixture compile error and all prior logs retained.

Native build, hardware touch/readability, selected Springfield dashboard rendering,
radio continuity and orientation are pending. No hardware acceptance claimed yet.
Next: one native build and COM17 app-only flash with a fresh settings backup;
leave C6, SD and COM39 unchanged. Preserve the user's current setup progress.

## COM17 installation and bounded startup result

Final firmware source: 81e9147db25f76cd0807241fc26dd8da3f0c8478
(UI changes 75a14959; P4 warning correction 57d1fcac; camera-independent manual-save
preservation 81e9147d). The manual polar-coordinate save and bottom-right save
button edge are covered by the final host tests.

Final host command:
`ctest --test-dir .local/maps/setup-ui-linux -R "^(map_pipeline|setup_flow|location_store)$" --output-on-failure`
from WSL Ubuntu. Result 3/3 PASS; log
`setup-list-linux-test-fallback-20261018.log` (build log with same fallback suffix).
The first native attempt rejected misleading indentation; corrected before flash.
Native build wrapper with verified world archive passed for final source:
`setup-list-native-build-final-20261018.log`. OrcMaps pin/miniz gates and 8 KiB
FT8 PSRAM placement gate PASS; reserve remains 40,960 bytes.

App-only flash: `python .local/maps/flash_com17_setup_list.py`.
COM17 identity 30:ED:A0:E2:E6:95 checked before each operation. Fresh backup and
manifest: `.local/maps/com17-recovery/setup-list-20261018-103208/`.
Backup NVS was verified against the device before writing. Previous app verified
against the sealed 329e5a9e candidate. New app verified; boot/table/NVS/PHY/core/world
verified unchanged after write. No C6 or SD write; COM39 untouched.
Logs at checkout root: `com17-setup-list-*-20261018-103208.log`.

App bytes: 5032096; SHA256 `f6731c897eed3efd3d5deca78db81ed57060f435a56122d00a59feb176f87f53`.
ELF SHA256 `168266eff5b1800cbd339fbb1361d0723002371876fee78aa8c04edc7468ef76`.

Captured controlled reboot: `com17-setup-list-boot-20261018.log`.
Matching ELF prefix 168266eff, BOOT_STAGE ready and summary sd=1 wifi=0 rtl=1.
Read-only diagnostics (no authentication/key change or UI navigation):
`python .local/maps/com39_bench.py .local/maps/com17-setup-list-public-health.json com17-setup-list-health-20261018.log --port COM17`.
Result: PASS. At uptime 70 seconds, DMA free=12,415, largest=11,776;
PSRAM free=22,223,024. Driver 0.9.3 streams Blog V4 at saved 1,090 MHz,
effective 2,047,847 samples/s, overruns=0, drops=0. This is bounded stream proof,
not RF decode acceptance. Host/cop C6 both remain 3.0.6, match=1.
Startup restored HOME in ADS-B band. Map frame bytes=0 in this observation;
selected Springfield rendering under active reception remains unverified.

Physical text readability, save-button response, map-list gestures, rotation and
ADS-B overlay appearance await the user's walkthrough. Existing valid saved
orientation wins over the new fresh-install default. No extra settings erase was
performed, and no wizard steps were advanced by automation.
Next milestone: physical acceptance and selected regional-map/ADS-B rendering,
then retained-map SD and active-radio regression checks. Overall integration is
not declared complete by the build/flash/startup results.

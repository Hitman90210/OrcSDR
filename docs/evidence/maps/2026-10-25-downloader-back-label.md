# Downloader navigation wording

The user reported that Local Maps → Get Maps used CANCEL to leave the screen,
although leaving does not cancel the download. The downloader header now says
BACK. While an operation is busy, its hint says:

> Back to the device: downloads continue. Stop Transfer pauses the download.

STOP TRANSFER remains the explicit transfer control. The change adds no new
action, allocation, touch target, persistence or transfer behavior. Picker/setup
cancellation outside the Get Maps view retains its existing label. Existing
serial `RTL_MAP CANCEL` closes the map screen; `RTL_MAP DOWNLOAD CANCEL` is the
distinct authenticated transfer-stop command. Serial behavior is unchanged.

The recording-display regression checks BACK, the visible explanation, continued
busy state after touching Back, and an unchanged transfer-cancel count. Existing
progress/verification assertions continue to verify no extra full-screen clears.

Run directory: `F:\AI\OrcSDR-TEMP\maps-integration-completion`.

- Focused CMake target `orcsdr_maps_tests`, then CTest `-R '^map_pipeline$'`:
  red failed on the old label; green PASS 1/1. Logs
  `map-downloader-back-{build,test}-{red,green}-20261025.log`.
- `python .local/maps/check_downloader_back_syntax_20261025.py map_screen.cpp`:
  real P4 syntax PASS, log `map-downloader-back-p4-syntax-20261025.log`.
- `git diff --check`: PASS.

Raw logs remain beside the invocation, outside Git. No hardware was accessed;
COM39's active download remains untouched. Native link/installation and physical
wording acceptance remain pending for the combined clock/navigation candidate.

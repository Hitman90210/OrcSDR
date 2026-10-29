# Manual map guidance — October 23, 2026

Source-only change; installed COM17 remains f38b8601. No build/flash or native
appearance acceptance is claimed. Public test map/catalog release assets were
rechecked read-only while answering the user's installation questions.

Prepared user guide: docs/user-guide/offline-maps.md, linked from the guide index,
Settings page and README in 56ad22c1. Covers both installation paths, exact public
pilot filenames, checksum verification, direct SD-root /orcmaps/ layout, Rescan/
selection and the difference between automatic full hashing and manual discovery's
metadata/size checks. It is a local unpublished guide, not an updated public site.
The initial link checker mistakenly treated an existing mailto link as a file;
corrected scheme handling passes 32 local targets. Final output is
maps-user-guide-link-check-20261023.log. No product source defect was inferred.

The Local Maps inventory now shows two static font-size-2 footer lines:

```text
Manual SD: copy matching .pmtiles + .manifest.json to /orcmaps/.
Files + checksum: github.com/hardcoreerik/OrcMaps/releases
```

They use the existing text/theme path at y=582 and 606, above the y=650 setup
navigation row. Existing status/error messages, active transfers and inventory
overflow/low-memory warnings take priority. No new page, control, buffer or
navigation action was added. Read-only RTL_MAP HELP includes the same folder/
filename information and HTTPS release URL.

Commands from the integration root:

```text
wsl -d Ubuntu -- bash -c 'cd /mnt/f/AI/OrcSDR-TEMP/maps-integration-completion && cmake --build .local/maps/feature-replay-integration-20261023 --target orcsdr_maps_tests -j2 && ctest --test-dir .local/maps/feature-replay-integration-20261023 -R ^map_pipeline$ --output-on-failure' > maps-manual-guidance-host-20261023.log 2>&1
python .local/maps/check_manual_guidance_syntax_20261023.py map_screen.cpp map_serial.cpp
```

Both exit 0. CTest passes 1/1 and executes the actual picker/Settings/map-screen
test code with the recording display. It verifies visible manual guidance,
read-only serial Help, guidance remaining visible through repeated background
byte/revision updates without increasing full-screen-clear count, plus existing
download/progress/verification redraw and navigation assertions. This is a
focused test, not the entire maps/engine suite or a physical flicker check.
P4 syntax for both source units passes; output
maps-manual-guidance-p4-syntax-20261023.log.

Remaining: compile/install a reconciled candidate and check physical text fit,
readability, status priority and setup footer spacing. Producer 6a621a7's
requested full-suite/publication approval remains pending, as does scoped
reconciliation with nine newer FT8 clock commits on origin/main. No source
publication, map upload or production-catalog promotion occurred here.

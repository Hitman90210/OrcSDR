# Location lookup identity and validation — October 22, 2026

## Reproduced defects and narrow correction

The actual lookup worker stored one current result but kept the last successful
address as a separate cache key. Address A → IP area → address A could return
the IP result as a cached address result. The new production-code test reproduces
this at `location_lookup_tests.cpp:51`. The correction acquires existing request
ownership before cache access and compares the requested address with `g_query`,
which identifies the currently published result. IP requests replace that query
with the empty string. Busy requests remain rejected and immediate repeats of
the current successful address remain cached. The obsolete last-address buffer
is removed; no public API or persistence format changes.

The worker also parsed address coordinate strings using `strtod` without checking
where parsing ended. Empty/nonnumeric strings could become zero and numeric
prefixes could be accepted. After the cache correction, the second regression
reproduces invalid data becoming ready at line 55. A small shared coordinate
parser now requires a nonempty complete numeric conversion; existing finite and
latitude/longitude range checks still apply. It does not write receiver location.

Changed production source: `apps/orcsdr-tab5/ui/location_estimate.cpp`. Tests:
`apps/orcsdr-tab5/tests/maps/location_lookup/` and two additional declarations
in the existing fake HTTP SDK header. The tests compile the real worker and
the SDK's actual cJSON source; only HTTP responses and task scheduling are faked.
No network service, Wi-Fi credentials, SD or Tab5 is accessed by these tests.

## Exact focused commands and results

From the integration worktree root, using WSL:

```text
cmake -S apps/orcsdr-tab5/tests/maps/location_lookup -B .local/maps/location-lookup-host-20261022 -DCJSON_ROOT=/mnt/c/Espressif/frameworks/esp-idf-v5.5.4/components/json/cJSON
cmake --build .local/maps/location-lookup-host-20261022 -j2
ctest --test-dir .local/maps/location-lookup-host-20261022 --output-on-failure
cmake -S apps/orcsdr-tab5/tests/maps/location_lookup -B .local/maps/location-lookup-asan-20261022 -DCJSON_ROOT=/mnt/c/Espressif/frameworks/esp-idf-v5.5.4/components/json/cJSON -DCMAKE_C_FLAGS="-O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer" -DCMAKE_CXX_FLAGS="-O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer"
cmake --build .local/maps/location-lookup-asan-20261022 -j2
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 ctest --test-dir .local/maps/location-lookup-asan-20261022 --output-on-failure
cmake --build .local/maps/labels-host --target orcsdr_map_http_stream_tests -j2
ctest --test-dir .local/maps/labels-host -R '^map_http_stream$' --output-on-failure
```

Normal lookup **1/1 PASS**, sanitized lookup **1/1 PASS**, shared HTTP adapter
**1/1 PASS**. Cases include cache method/query identity, same-address cache hit,
busy rejection, empty/nonnumeric/partially numeric/nonfinite/out-of-range
coordinate rejection, HTTP 503 and task-creation failure/retry. Production task
stack/core/caps and HTTPS bundle/User-Agent configuration are checked at the
fake boundary; real scheduling/TLS/memory are not exercised.

P4 syntax command:

```text
python .local/maps/check_location_lookup_syntax_20261022.py
```

**PASS**, using the native compile database. Logs in the worktree root:
`location-lookup-red-{configure,build,test}-20261022.log`,
`location-lookup-coordinate-red-{build,test}-20261022.log`,
`location-lookup-host-final-{build,test}-20261022.log`,
`location-lookup-asan-{configure,build,test}-20261022.log`,
`location-lookup-asan-final-{build,test}-20261022.log`,
`location-lookup-http-adapter-{build,test}-20261022.log` and
`location-lookup-p4-syntax-20261022.log`.

## Separate production geocoding gate

Source inspection also confirms the current address service URL is hard-coded
to public Nominatim. Its official policy, checked October 22, states an aggregate
application limit of one request per second and requires apps to be able to
switch services without a software update. It also specifies identifying
User-Agent, attribution, caching and no autocomplete/systematic querying.
[Official policy](https://operations.osmfoundation.org/policies/nominatim/).

That public endpoint must not be treated as the production solution for the
20,000-user scenario without resolving those constraints. No live address query
was sent in this investigation. The deterministic fixtures above exercise the
worker's address/IP sequence without calling any provider. Production provider
or owned proxy selection and runtime service-switch configuration remain open;
they are separate from the map-archive GitHub pilot approval. Do not substitute
a generic hard-coded service or promise that offline packs include address search.

Native linking, device boot/IP lookup/failure handling, live address service
acceptance and complete self-contained setup remain unproven by these host checks.

## Native and COM17 bounded follow-up

Installed source commit: `f61bdad5`. Commands from the integration worktree root:

```text
python .local/maps/build_fit_order_20261020.py build location-lookup-20261022
C:/Espressif/python_env/idf5.5_py3.14_env/Scripts/python.exe .local/maps/inspect_location_lookup_20261022.py
python .local/maps/com17_label_bench.py location-lookup-preflash-plan-20261022.json location-lookup-preflash-device-20261022.log
python .local/maps/flash_location_lookup_20261022.py
python .local/maps/com17_label_bench.py location-lookup-setup-device-plan-20261022.json location-lookup-setup-device-20261022.log
```

All commands exit 0. Device identity COM17 `30:ED:A0:E2:E6:95` is verified.
The installed previous `7b2b1228` app, bootloader/table and recovery are checked
before app-only writing. Fresh non-app backups and post-write preservation pass;
NVS/world/C6 and existing SD files are preserved. COM39 is untouched. Artifact:
`.local/maps/location-lookup-f61bdad5-20261022/`; rollback is `7b2b1228`.
App size 5,139,488 bytes; factory headroom 1,151,968 bytes.
App SHA-256: `c186cefef8750dde0ad64020dc65779b4a355dd70205611691bae6d003320baf`.
ELF SHA-256: `f3bc730f2bbce489aa0a3beb2fce14119bc60c5b98b6a3cb691264d3ae4b3788`.
Existing symbol/pin checks, PSRAM placement, diagnostics OFF and unchanged
40 KiB DMA reserve pass. The catalog remains unconfigured; no keys are generated.

Logs in the root: `fit-order-native-location-lookup-20261022-20261020.log`,
`location-lookup-artifact-inspection-20261022.log`, preflash device/console logs,
`location-lookup-{verify-previous,backup-*,verify-backups,app-write,preservation}-20261022.log`,
`location-lookup-setup-device-20261022.log` and
`location-lookup-setup-console-20261022.log`.
Native monitor was invoked from `apps/orcsdr-tab5`; its saved
`location-lookup-boot-native-monitor-20261022.log` reaches SD/Wi-Fi/RTL Ready
and matching app startup. It is closed before serial command testing.

The bounded rerun enters setup from ADS-B, visits Network, requests a scan
through the visible button and remains connected with four APs reported.
The driver is IDLE during setup. Location's visible network-area button sends
one IP lookup; `RTL_LOCATION STATUS` reports `busy=0 ready=1` and
`Approximate IP area; confirm to save`. The proposal is not confirmed or saved.
Every saved-location readback retains the run's original E7 coordinates
`440628866,-1230160273`.

Back abandons the lookup view; Next reaches Maps. The catalog reports
`configured=0`, correctly reflecting pending publication/configuration approval.
Next reaches Complete, with the driver still IDLE. Finish persists setup progress
and returns to ADS-B with reception STREAMING. The evidence checker verifies
these markers and preserved coordinates in
`location-lookup-setup-evidence-check-20261022.log`.

Final observation: ADS-B 1090 MHz, AUTO gain 0, RTL AGC off; approximately
2,046,890 samples/s with zero observed driver drops/overruns. Free PSRAM
20,648,320 bytes, largest block 20,447,232; largest DMA block 7,168; main-stack
high-water margin 4,172; uptime 133,885 ms. No unexpected reset is recorded.
All monitor/test sessions are closed. This is bounded streaming evidence,
not antenna-qualified aircraft decoding or sustained stability acceptance.

The address/IP/address cache sequence and malformed-response rejection remain
host-tested; no live address query or deliberately malformed live provider was
used on P4. Entering new Wi-Fi credentials, offline hardware lookup failures,
first boot/interrupted progress/hotplug, complete address-provider acceptance,
physical appearance, signed HTTPS map installation and performance gates stay
open. The map-download pilot still awaits explicit publication approval.

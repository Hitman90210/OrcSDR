# Signed map catalog consumer — source and focused checks

Files: `apps/orcsdr-tab5/ui/map_catalog.hpp/.cpp`, native main CMake source
registration and `tests/maps/catalog_security/` (CMake, C++ runner and Python
fixture/signing driver). No HTTP/SD/NVS calls, task startup or UI changes have
been added. There is no configured production public key or endpoint.

`read_signed` verifies exact received bytes using Mbed TLS P-256/SHA-256 before
JSON interpretation. It uses the existing JSON allocator without changing
global hooks. The source SDK configuration still allows normal allocations
to fall back to PSRAM and reserves 40,960 internal bytes for DMA; this change
does not modify either setting. Worker memory budgets remain to be measured.

The consumer bounds payload/signature/key sizes, nesting, DOM nodes, object
members, packs and mirrors. It rejects duplicate decoded keys, escaped NUL,
unknown outer fields/schema, invalid engine manifests, duplicate pack identity,
artifact size/hash mismatch and unsupported URL forms. Pack validation reuses
the exact pinned OrcMaps public parser/validator rather than reimplementing
its schema or confusing PMTiles with legacy ORCMAP1.

Freshness checks require caller-supplied trusted UTC, refuse expiry and generation
more than five minutes ahead, and compare the catalog revision with the supplied
previously accepted revision/hash. Equal revisions require byte-identical payload
hashes. **NVS persistence and the caller's clock-trust decision are not implemented
by this API.** Existing offline maps need neither this catalog nor a clock.
Output is replaced only after complete success; rejected input retains the
previous catalog. JSON allocation failure and C++ bad allocation are handled,
but comprehensive device allocation-failure testing remains open.

## Actual tests

Initial focused configuration failed because the new implementation did not
exist yet (`map-catalog-consumer-red-20261019.log`). Host compilation then
required linking Windows `bcrypt`, used by the existing Mbed TLS library.
The NUL test initially mutated a word absent from the Springfield fixture;
it was corrected to mutate the actual `display_name` field before acceptance.

Focused host project:

```powershell
cmake -S apps/orcsdr-tab5/tests/maps/catalog_security -B .local/maps/catalog-security-20261019 `
  -DORCMAPS_ROOT=F:/AI/OrcSDR-TEMP/maps-integration-completion/apps/orcsdr-tab5/managed_components/orcmaps `
  -DMBEDTLS_ROOT=C:/Espressif/frameworks/esp-idf-v5.5.4/components/mbedtls/mbedtls `
  -DMBEDCRYPTO_LIBRARY=F:/AI/OrcSDR-TEMP/maps-integration-completion/.local/orcdial-security/mbedtls/library/Debug/mbedcrypto.lib `
  -DCJSON_ROOT=C:/Espressif/frameworks/esp-idf-v5.5.4/components/json/cJSON
cmake --build .local/maps/catalog-security-20261019 --config Debug
python apps/orcsdr-tab5/tests/maps/catalog_security/run_cases.py `
  --exe .local/maps/catalog-security-20261019/Debug/map_catalog_security_test.exe `
  --openssl "C:/Program Files/Git/usr/bin/openssl.exe" `
  --manifest F:/AI/OrcSDR-TEMP/orcmaps-quality-delivery/.local/global-assessment/global-z8-candidate.manifest.json
```

Final result: **29 signed consumer cases PASS**, using the actual global
manifest, actual resolved cJSON and existing host-built Mbed TLS 3.6 library.
The fixture runner creates an ephemeral P-256 key, signs each intended invalid
case, verifies expected error classes and checks unchanged output on rejection.
It also injects JSON allocation failure. Tampering of an invalid JSON document
is rejected at signature verification, before parsing. Temporary private keys
are removed when the runner exits. It reads metadata, not the 543 MB archive.

The P4 compiler also accepted the new source using the existing native
`catalog_sync.cpp` compile flags (SDK 5.5.4):

```powershell
python .local/maps/check_map_catalog_syntax_20261019.py
```

Result: **syntax exit 0**. The only displayed warning came from an existing SDK
`periph_ctrl.h` macro. This is not a complete native link/build or device test.

Saved logs in the OrcSDR invocation directory:
`map-catalog-consumer-{red,configure,build,cases,global,p4-syntax}-20261019.log`.
The earlier `cases` log covers 25 Springfield cases; the final `global` log
covers the expanded 29 cases. No tests were run against COM17 or COM39.

Direct producer/consumer interoperability also passed: the native consumer
test executable accepted `.local/catalog-smoke-20261019/TEST-ONLY-output/`
`maps-catalog-v1.json` and its detached signature with that smoke run's saved
test public key. This is the actual previously generated OrcMaps payload,
not the fixture driver's JSON serialization. Exit 0 is recorded in
`map-catalog-consumer-interoperability-20261019.log`; it does not establish
production trust because the endpoint and key are explicitly test-only.

## Producer alignment and remaining gate

OrcMaps's builder now enforces the same plain-ASCII URL path/DNS-host, UTC
year and JSON structure limits. Added regressions reproduced the producer
accepting unsupported forms before the correction; **10 focused builder tests
pass**. OrcMaps root logs:
`map-catalog-consumer-contract-red-20261019.log`,
`map-catalog-structure-red-20261019.log`,
`map-catalog-consumer-contract-green-20261019.log`.

The next delivery milestone is the shared worker that fetches bounded catalog
bytes, invokes this API with the approved trust key and persistent state, and
performs resumable verified archive staging/activation. Network, removable SD,
power-loss recovery, memory/radio continuity, wizard download controls and
production trust/hosting remain unverified. Neither Tab5 changed, and no source
was pushed, merged, tagged or published by this step.

# Explicit nonexpiring catalog — source validation

User explicitly requests production delivery without catalog expiry. Source
baseline `811988b5` on `codex/maps-integration-completion`, no tracked changes
before this scope. OrcMaps baseline `6a621a7`, clean before builder changes.
No PR review/main merge or unrelated renderer change. Risk MEDIUM: signed
catalog interpretation changes, while integrity and recovery rules remain.

Contract: required JSON `expires_at: null` means no expiry; consumer represents
it as `expires_at=0`. Existing dated catalogs still expire as before. Missing,
empty, boolean, numeric and invalid date values are not non-expiry. A valid
clock and generation-time check remain required, along with signed payload,
revision+hash rollback and archive validation. Installed offline maps never
depended on catalog expiry and remain usable.

Modified OrcSDR files: `map_catalog.cpp/.hpp`, `map_install.cpp`, signed-case
driver `run_cases.py`, and transfer tests. OrcMaps modifies the host catalog
builder and its focused Python tests; no consumer dependency bump is needed
for this host-only tool change. No managed source edits.

## Actual commands and results

OrcMaps root:

```text
python -m unittest discover -s tests -p test_map_catalog_builder.py
```

13 tests pass. Red log `maps-nonexpiring-builder-red-20261024.log` reproduced
explicit-null rejection before implementation; green log has same prefix with
`green`. Logs remain beside invocation in that worktree.

OrcSDR integration root:

```text
cmake --build .local/maps/catalog-security-20261019 --config Debug --target map_catalog_security_test map_transfer_test
python apps/orcsdr-tab5/tests/maps/catalog_security/run_cases.py --exe .local/maps/catalog-security-20261019/Debug/map_catalog_security_test.exe --openssl "C:/Program Files/Git/usr/bin/openssl.exe" --manifest F:/AI/OrcSDR-TEMP/orcmaps-quality-delivery/examples/m5stack-tab5/test-pack/springfield-97477.manifest.json
ctest --test-dir .local/maps/catalog-security-20261019 -C Debug -R ^map_transfer$ --output-on-failure
python .local/maps/check_nonexpiring_syntax_20261024.py map_catalog.cpp map_install.cpp map_catalog_session.cpp map_downloads.cpp
```

45 signed consumer cases pass, using temporary test keys only. Checks include
null-expiry acceptance and retained clock/future/rollback rejection, malformed
expiry, signatures and original dated catalogs. Transfer test 1/1 passes
(7.48s), including installation far after generation for a nonexpiring accepted
catalog; dated expiry still rejects before network access. Four P4 syntax checks
pass. Red consumer and installer tests failed for their specific new requirement
before implementation. No broad suite needed or run.

Logs beside invocation: `maps-nonexpiring-consumer-{red,build,green}-20261024.log`,
`maps-nonexpiring-install-{red-build,red,green}-20261024.log`,
`maps-nonexpiring-p4-syntax-20261024.log`.

## Production and remaining gates

The [concrete unsigned production package](../../maps-delivery/2026-10-24-production-catalog/README.md)
reuses the verified immutable world archive. No production key creation,
signature, publication, archive transfer, build or device operation in this step.
COM17 remains physically accepted `77a8cdce`; COM39 untouched. Native configured
production firmware, migration without lowering trust, public asset verification,
non-expiring signed-catalog device Check and release-package inclusion remain.

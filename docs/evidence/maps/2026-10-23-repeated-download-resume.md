# Repeated download interruption: focused host proof

User requested resume be included in the fresh GitHub-to-COM17 installation
test. Physical card preparation remains pending: this host check does not
substitute for that requested device transfer or its timing.

Changed only the synthetic transfer harness and user instructions:
`apps/orcsdr-tab5/tests/maps/catalog_security/transfer_test.cpp` and
`docs/user-guide/offline-maps.md`. No firmware implementation or installed app
changed. The Stream fake now counts bytes returned to the actual staging code.

Extended the cancellation case to interrupt, resume and interrupt again. Check:

- The second request starts at the exact saved prefix length.
- The next checkpoint grows and retains the previous prefix byte-for-byte.
- The final request starts at that second checkpoint and reads only the remainder.
- The resulting verified archive equals the complete source byte-for-byte.

Existing cases in the same program also exercise network failure, changed ETag,
unsynced tails, corrupt checkpoints, SD write/read failures and activation cuts.
These use a tiny PMTiles fixture padded to approximately 2 MiB, not the large
global archive, live GitHub, real power loss or a physical SD card.

From the integration root, the initial explicit ESP-IDF CMake 4.0.3 invocation
failed before compiling because it cannot create the saved Visual Studio 18
2026 generator. Log `maps-repeated-resume-host-20261023.log`; not a test failure.
The installed host CMake 4.3.3 supports the existing generator:

```text
cmake --build .local/maps/catalog-security-20261019 --config Debug --target map_transfer_test
ctest --test-dir .local/maps/catalog-security-20261019 -C Debug -R '^map_transfer$' --output-on-failure
```

Build exit 0; selected test 1/1 passed in 4.76 seconds.
Log `maps-repeated-resume-host-corrected-20261023.log` beside invocation.
Only disposable fixed host-fixture files were used. No hardware operation.

Next gate: a clean device transfer, interruption/resume, complete integrity and
activation, then reboot/offline rendering. Report active transfer time, pause
duration, verification/activation and total elapsed separately, with polling
resolution. None of those native timing results is supplied by this host test.

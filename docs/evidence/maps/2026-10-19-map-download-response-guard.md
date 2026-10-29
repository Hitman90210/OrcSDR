# Map-download response guard — October 19, 2026

## Scope and scout

OrcSDR branch codex/maps-integration-completion, parent 0d45af67. The historical
catalog_sync downloader restarts at byte zero, removes staging on failure and
validates the original catalog's formats, including ORCMAP1. It does not expose
HTTP resume validation for OrcMaps PMTiles. No existing shared resume guard was
found in the bounded catalog/map/byte-range inspection. The legacy downloader
was left unchanged; an arbitrary PMTiles archive must not be passed to its
legacy map entry.

New portable source: ui/map_download_response.hpp, with focused host tests and
a registered map_download_response CTest target. This is a prerequisite for the
shared map-acquisition adapter, **not a working downloader or trusted catalog**.
No existing dashboard, SD file, native firmware or device was changed.

## Implemented contract

Resume preflight compares the authenticated catalog's expected byte count/hash
with the saved staging identity, requires a nonempty incomplete synced prefix,
and requires a bounded strong entity tag. Case differences in hexadecimal digest
text do not change identity; ETag comparison remains case sensitive.

Response acceptance requires the exact requested body length, unencoded bytes,
and either a complete fresh 200 response or an exact single-range 206 response.
A whole-file 200 response must never append to a saved prefix. Chunked/ranged
continuation requires strong object identity; malformed/wildcard/multipart,
different-total and overflowing ranges are rejected. ETag and Content-Range
inputs are capped at 128 bytes. No allocation occurs inside the guard.

These policies follow the semantics of If-Range and Content-Range, with deliberate
installer restrictions: require known lengths and do not support date validators
or multipart ranges. See [RFC 9110 §13.1.5](https://www.rfc-editor.org/rfc/rfc9110.html#name-if-range)
and [§14.4](https://www.rfc-editor.org/rfc/rfc9110.html#name-content-range).

The caller must still authenticate metadata, validate TLS and every redirect,
reject duplicate/truncated headers, stream/count actual body bytes, sync SD and
resume records, rehash staging bytes, verify final SHA-256 and PMTiles/manifest
compatibility, and atomically activate only a complete verified pack. The guard
never deletes, truncates or activates files. Header acceptance does not prove
body integrity. No production call sites exist yet; it must be wired into the
new adapter before downloads can be offered to users.

## Executed verification

Invoked from F:/AI/OrcSDR-TEMP/maps-integration-completion; all logs are there.

    cmake --build .local/maps/timing-host-20261019 --config Release --target orcsdr_map_download_tests
    ctest --test-dir .local/maps/timing-host-20261019 -C Release -R "^map_download_response$" --output-on-failure

Before implementation: expected compile failure for the missing header, saved
as map-download-response-red-20261019.log. After implementation: PASS, 1/1,
map-download-response-build-20261019.log and map-download-response-tests-20261019.log.
After read-only review suggested exact input-limit tests: PASS, 1/1,
map-download-response-reviewed-build-20261019.log and
map-download-response-reviewed-tests-20261019.log. Tests cover changed artifact
identity, missing/weak/changed ETags, 128/129-byte boundaries, whole-response
fallback, status errors, lengths, encoding, malformed ranges and uint64 limits.

Standalone P4 compiler syntax check, PASS (no SDK link or firmware build):

    C:/Espressif/tools/riscv32-esp-elf/esp-14.2.0_20260121/riscv32-esp-elf/bin/riscv32-esp-elf-g++.exe -std=c++17 -fsyntax-only -I apps/orcsdr-tab5/ui apps/orcsdr-tab5/tests/maps/map_download_response_tests.cpp

Log: map-download-response-p4-syntax-20261019.log. The bounded subagent review
found no correctness blocker; it was not a whole-branch review.

## Remaining milestone

Connect authenticated map catalog entries to an asynchronous HTTP/SD adapter
using this guard, then test real interruption, cancellation, power loss, removed/
full SD and object replacement without losing previous working packs. Provider
selection/publication remain separate approvals. Existing native/COM17 timing
and OrcMaps full-regression approval requests remain pending; this source work
does not authorize those operations. COM39 remains Claude's Dead Air device.

# OrcMaps merged; OrcSDR source pin updated

User approved development push/consumer checks and work to merge OrcMaps,
then explicitly approved the whole-branch review. No OrcSDR main merge,
tag/release, map upload, native firmware build or device flash in this step.

## Verified upstream merge

PR https://github.com/hardcoreerik/OrcMaps/pull/6 merged into main at
`722d0d382f99b2887694af333f85b1ee048f4208` (October 18 Pacific time).
Fetched origin/main and verified that commit is its ancestor. The merge tree
exactly matches reviewed head `15c1acf233f7d6410ebdf3bcf7134052367edac4`.
No force push, branch deletion or worktree cleanup. Existing PR #5 was not changed.

Seven configured host CTest programs passed at dbb1b98; later engine changes
were documentation only. Independent read-only whole-branch review found no
actionable BLOCKER/FIX BEFORE MERGE findings. Optional future coverage: strokes
across tile seams at fractional camera origin. No verified defect was identified.
CodeRabbit reports skipped/manual review required; no completed CodeRabbit
review is claimed. Review record: `merge-review-20261018.md` in engine worktree.

All premerge CI gates passed at reviewed head: Documentation Truth, Data
Provenance Truth, ESP-IDF miniz link esp32p4 and esp32s3. Initial documentation
failure was stale STATUS.md counts, corrected to 182 defined functions and
seven configured CTest programs; local checker reports zero errors/warnings.
This does not include optional font backends or full global/device acceptance.

Engine worktree logs/artifacts: `pack-profile-full-build-20261018.log`,
`pack-profile-full-tests-20261018.log`, `merge-doc-truth-20261018.log`,
`merge-pr-state-20261018.json`, `merge-tree-verification-20261018.json`.

## Consumer source and focused verification

Changed `apps/orcsdr-tab5/main/idf_component.yml` to merged revision 722d0d3;
component manager regenerated `dependencies.lock`. It confirms all unrelated
dependencies unchanged. Requested/locked/fetched identity check passes at full
merge SHA. No hand edits under managed_components.

Added `tests/maps/map_inventory_tests.cpp` coverage through the actual OrcSDR
Inventory adapter: explicit Protomaps-v4 manifest is discovered/resolved, unknown
Protomaps version is still refused as compatibility failure. Existing paths and
integrity gates remain unchanged.

Focused consumer commands (WSL, integration worktree root):

```sh
cmake --build .local/maps/preview-pinned --target orcsdr_maps_tests orcsdr_map_preview_tests orcsdr_map_timing_tests
ORCSDR_LABEL_TEST_ARCHIVE=/mnt/f/AI/OrcSDR-TEMP/orcmaps-quality-delivery/.local/global-assessment/global-z8-candidate.pmtiles ctest --test-dir .local/maps/preview-pinned -R '^(map_pipeline|map_preview|map_timing)$' --output-on-failure
```

Result: 3/3 pass against c39f827. Only STATUS.md differs between c39f827 and
the final merged pin; verified engine source, headers, adapters, build manifests,
third-party code and tests are identical. Avoided a redundant rebuild for that
documentation-only difference. Final pin identity check run after resolving 722d0d3:

```sh
python apps/orcsdr-tab5/tools/check-orcmaps-pin.py apps/orcsdr-tab5
```

Root logs: `pack-profile-pin-resolve-20261018.log`,
`pack-profile-pin-merged-20261018.log`, `pack-profile-consumer-build-20261018.log`,
`pack-profile-consumer-tests-20261018.log`. Lock backups under
`.local/maps/pack-profile-pin-c39f827-20261018/` and
`.local/maps/pack-profile-pin-merged-20261018/`.

Current devices unchanged: COM17 still runs the physically accepted graphics
fix/setup-pause build using earlier OrcMaps pin 63f0573; COM39/C6/SD/world untouched.
Consumer changes are not yet committed at this evidence checkpoint.

Remaining: native build and device global-pack validation, real pack manifest/hash
and installation, boundary/font quality, cached-motion timing, trusted delivery
and complete self-contained setup. Overall integration plan remains incomplete.

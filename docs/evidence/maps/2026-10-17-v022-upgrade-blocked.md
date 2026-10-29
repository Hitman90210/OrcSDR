# OrcMaps v0.2.2 upgrade scout: exact supplied revision is blocked

Superseded by published v0.2.3: [upgrade verification](2026-10-17-v023-upgrade-result.md).
The lines diverged at 14cdd41; e3028c60 did not historically revert c3cc09de.
The findings below describe replacing the pin with that earlier divergent line.

Date: 2026-10-17, America/Los_Angeles.
OrcSDR branch `codex/maps-integration-completion`, source HEAD `73f84ce7`.
Current engine pin `c3cc09de3882149e03c7c00d7897c6882b324c6b`.
Requested engine `e3028c60b722729a1fe7ef95341ac9c525edca26`.

## Verified findings

The supplied commit exists in the local OrcMaps repository and its
`idf_component.yml` reports 0.2.2. Its existing five-argument RenderFeatureAt
declaration remains, with the additive RenderScratch overload. OrcSDR uses that
existing call. A targeted search found no OrcSDR-owned TileScreenMap definition
or manually populated Q16 fields.

However, the supplied commit is not a descendant of OrcSDR's current pin:

```text
git merge-base --is-ancestor c3cc09de3882149e03c7c00d7897c6882b324c6b e3028c60b722729a1fe7ef95341ac9c525edca26
exit 1
```

Comparing the exact commits shows two material regressions relative to the engine
currently integrated into OrcSDR:

1. **Polygon rings:** `include/orcmap/render_target.hpp` no longer declares
   FillPolygonRings. The renderer's polygon case explicitly takes
   `feature.geometry.paths[0]` and calls FillPolygon, with a comment saying extra
   paths are ignored. Current c3cc09de instead renders exterior parts and holes
   through FillPolygonRings. The requested revision therefore lacks the reviewed
   polygon-ring fix that addressed missing world landmasses. The claim that no
   public API or rendering behavior differs is not true relative to this pin.
2. **Allocation-safe cleanup:** `src/tiles/mvt_stream.cpp` replaces the current
   allocation-free vector swaps with a feature-byte vector copy and
   shrink_to_fit during scratch cleanup. This loses the cleanup fix already
   retained for allocation-failure handling in the OrcSDR integration. No new
   device allocation-failure test was run; this finding is a source comparison.

Remote check against `https://github.com/hardcoreerik/orcmaps.git`:

```text
git ls-remote origin 'refs/tags/v0.2.2*' 'refs/heads/main'
14cdd418fe9101d56ea904eaa8c4da695a7fed9d refs/heads/main
```

No matching remote v0.2.2 tag was returned at this check. The local v0.2.2 tag
points at e3028c60. Do not conflate local tag presence with remote publication.

## Outcome and next step

Risk HIGH for a direct replacement: required polygon and allocation-safety fixes
would be lost. Stopped before editing the pin or lockfile, resolving components,
building or flashing. No managed component was removed or hand-edited; no
vendored renderer or workaround was introduced. The existing symbol-verification
and dependency-agreement gates remain unchanged.

No compilation, pixel comparison or hardware performance test was run. This is
not an observed new crash or a measured pixel-difference result; the source
incompatibility is sufficient to prevent the proposed zero-behavior-change
upgrade. No speed claim is made. COM17, COM39 and C6 untouched.

Required next milestone: an upstream revision combining the v0.2.2 hot-path
changes with the polygon-ring and allocation-free cleanup fixes, then publication
of that revision. Do not fix this by copying changes into OrcSDR. With a corrected
pin, regenerate the lockfile through the component manager, compare identical
fixture views against the current engine and run the requested native build.

Scout log in the invocation directory:
`F:/AI/OrcSDR-TEMP/orcmaps-integration-completion/maps-v022-upgrade-scout-20261017.log`.
Copies are retained in the OrcSDR integration directory and beside this report.

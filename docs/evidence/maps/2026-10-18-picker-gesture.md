# Picker gesture correction — focused host evidence

Phase 3, October 18. Source-only change in the existing map_screen.cpp and
map_screen_tests.cpp; preserved wizard edits in those files remain uncommitted.
No device or engine pin changed.

Observed cause: a map press immediately recentered the camera; subsequent
drag motion panned from that changed center. Touch-down could therefore queue
a frame immediately superseded by the first drag event. This is an interaction
defect, not evidence that decoding/cache performance is solved.

Correction: hold the initial map point until release. A tap recenters on
release; movement of at least six pixels starts a drag using the existing
camera. Smaller jitter does not queue a camera change. Release outside the map
cancels a tap. Opening another map screen clears pending gesture state.

Test-first reproduction: the assertion that touch-down leaves longitude
unchanged failed in `picker-gesture-red2-20261018.log`. After correction, the
focused map_pipeline CTest entry passed, including real map-screen tests.
Expanded checks for small jitter and outside release also passed **1/1**.

Commands from the integration worktree under WSL:

```
cmake --build .local/maps/labels-host --target orcsdr_maps_tests -j4
ctest --test-dir .local/maps/labels-host -R '^map_pipeline$' --output-on-failure
```

Logs: `picker-gesture-green-build-20261018.log`,
`picker-gesture-green-20261018.log`, `picker-gesture-edge-build-20261018.log`,
`picker-gesture-edge-20261018.log`. Earlier exploratory red logs are preserved.

Native compilation, physical drag acceptance, update percentiles and active-RF
gesture tests remain unverified. No speed multiplier or 33 ms claim follows
from this correction. Next: bounded cached-motion/display-transfer work.

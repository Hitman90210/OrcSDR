# Native setup navigation without saving

Device: COM17, USB serial `30:ED:A0:E2:E6:95`; installed source `137e244a`,
engine `2133547`. Routine bounded native validation is authorized. No flash,
reset, deletion or map installation in this check; COM39 untouched.

The source scout confirmed `checkpoint()` writes progress only for automatic
setup; manual reruns keep it in RAM until Finish. The test deliberately never
requests successful Finish, edits a location, changes network credentials or
selects a different map.

From the integration root:

```text
python .local/maps/com17_label_bench.py maps-setup-navigation-plan-20261024.json maps-setup-navigation-device-20261024.log
python .local/maps/verify_setup_navigation_20261024.py
```

Both exited 0. The actual firmware serial handlers verified this step sequence:
`0,0,1,0,1,2,3,4,3,2,1,0,4,0` where Welcome=0, Network=1,
Location=2, Maps=3 and Complete=4. This covers:

- Manual entry and rejection of Finish at Welcome.
- Next/Back and Skip through the entire sequence.
- Back through the entire sequence.
- Quick Start from Welcome, Cancel, and reopening at Welcome.

All 14 snapshots report setup active with the expected step. Every saved-location
snapshot remains configured at E7 `440628866,-1230160273`. Final driver IDLE,
effective_sps=0, drops/overruns=0. Final health: uptime 973,641 ms, free PSRAM
3,644,360 bytes / largest 3,473,408; largest DMA 20,480; main stack HWM 1,564.
Uptime continued; no reset requested. Final state is paused setup Welcome.

Logs saved beside invocation: `maps-setup-navigation-device-20261024.log`,
`maps-setup-navigation-console-20261024.log`,
`maps-setup-navigation-verification-20261024.log`; exact plan
`maps-setup-navigation-plan-20261024.json` and machine-checked result
`maps-setup-navigation-summary-20261024.json`.

This proves named-command navigation/state transitions on the native candidate.
It does not prove touch appearance, successful Finish/reboot persistence, address
or network lookup accuracy, keyboard entry, no-SD behavior, interrupted first
boot, audio restoration or paused physical hotplug. Those gates remain open.
The pending user-requested fresh GitHub install/resume/timing test still needs
card preparation; this check does not replace it.

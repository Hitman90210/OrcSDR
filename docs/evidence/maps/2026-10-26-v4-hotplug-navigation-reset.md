# V4 unplug/navigation reset observation — October 26, 2026

Status: **unresolved release blocker**, separate from the completed V4 conducted
SSTV image checks. No root cause or permanent fix is claimed.

## Observed sequence

COM17 `30:ED:A0:E2:E6:95` ran app `fd8b6de6`, driver 0.9.3, Blog V4 profile 1.
Its three conducted SSTV modes had completed and saved successfully. The
operator was asked to wait for a recovery monitor before starting five cycles.

During authenticated preparation, the operator unplugged/replugged early and
later confirmed navigating to ADS-B without pressing Reset. The preparation
log records:

- 18:05:58 PDT: `SSTV TUNE 145500000` accepted, then bulk transfer errors,
  USB disconnect, bias-safe-off, and a queued stop completing successfully.
- A following gain request returned `ESP_RTL_SDR_ERR_UNSUPPORTED` while no
  tuner was available. The driver reported IDLE/profile 0; the preparation's
  identity guard failed. This was not a successful preparation or a scored
  recovery cycle.
- The first read-only recovery monitor subsequently encountered a serial write
  timeout. Its manifest records ERROR with zero cycles.
- A fresh read-only query then succeeded: connected Blog V4, ADS-B at 1090 MHz,
  STREAMING/AUTO, driver drops/overruns zero, uptime **76,678 ms**, reset reason
  **7**, instead of the preceding multi-hour uptime. The operator confirmed no
  manual reset and that ADS-B was their navigation action.

The unexpected restart is a real observation. Timing overlapped physical
unplug/replug, serial tuning/preparation and user navigation; this is not an
isolated reproducer that assigns the cause to one of those actions.
Reset reason 7 does not establish which task or subsystem failed. The firmware
also normalizes some CPU-level resets through an RTC-watchdog system reset;
without the original boot/panic output, the displayed reason alone is not a
complete original-fault diagnosis.

## Evidence preservation

The full 256 KiB coredump partition was read at 0x610000 using esptool. That
required an explicitly described controlled reset; no flash write or erase was
performed. Target identity and the saved matching ELF hash were checked first.

Dump SHA-256:
`68a19d94cf082dd0680c8d872cdd2a877c960776409641945ca55a42c968b3ca`.
It is byte-for-byte identical to the pre-fix backup and the earlier stale dump.
It therefore supplies **no new backtrace for this incident**. Do not decode it
against the current ELF and present the old incident as the cause of this one.

After readback, a native monitor was attached with `--no-reset`; controlled
restart output reports reset reason 11. That later reset is not another
unexplained watchdog event. Boot/radio startup succeeded. No C6 update, NVS
erase, SD overwrite or COM39 access was performed.

Logs beside invocation in the integration worktree:

- `release-v4-sstv-recovery-start-20261026.log`
- `release-v4-sstv-com17-five-cycle-recovery-20261026.log`
- `release-v4-sstv-recovery-readonly-recheck-20261026.log`
- `release-v4-sstv-com17-coredump-read-20261026.log`
- `release-v4-sstv-watchdog-after-readback-native-20261026.log`

Local binary/result artifacts remain under `.local/maps/release-v4-sstv-*`.
Pluto was muted and its mute readback passed; no RF source was active during
this recovery preparation or reset observation.

## Next controlled checkpoint

A fresh preparation restored SSTV/NFM, 145.500 MHz, manual gain 40.2 dB, bias
off, profile 1 and STREAMING. The new monitor prints BASELINE_READY only after
validating this state, records disconnect/reconnect plus increasing stream
bytes, rejects gain/profile/frequency changes and unexpected restarts, and exits
nonzero on incomplete recovery. Five cycles were then requested with the screen
unchanged. The five-minute recording ran from 18:14:51 through 18:19:51 PDT,
captured no disconnects, and ended INCOMPLETE with zero verified cycles and a
nonzero process exit. No cycle count can be inferred from that recording.

The operator subsequently reported: "It worked as i intened. 3 cycles. reception
returned". Record three **operator-reported** recoveries, not five passes and
not three automatically verified cycles. Their exact timing relative to the
recording window was not established. The original five-cycle gate remains
incomplete; no hardware/software limitation is claimed as UNSUPPORTED.

A read-only end-state query at 18:23:09 PDT showed uptime 772,348 ms, reset
reason 11, profile 1 STREAMING at 145.500 MHz, manual 40.2 dB, bias/RTL AGC off,
zero driver drops/overruns and a healthy logger with zero dropped records.
SSTV was inactive and the observed stream was about 2.4 MSPS, rather than the
240 kS/s SSTV baseline. This proves the end state, not each cycle's configuration
or protocol decoding. Uptime remained continuous since the controlled restart.

Evidence: `release-v4-sstv-com17-five-cycle-recovery-retry-20261026.log`,
`release-v4-sstv-three-cycle-readback-20261026.log`, and local manifests
`.local/maps/release-v4-sstv-recovery-progress-retry-20261026.json` and
`.local/maps/release-v4-sstv-three-cycle-operator-record-20261026.json`.

Passing those isolated cycles would not erase the mixed navigation/unplug
failure above. That sequence still needs investigation before full release-gate
approval.

## Navigation-only isolation

After the operator's three-cycle report, three authenticated SSTV → ADS-B
navigation passes completed without a physical disconnect or active RF source.
Each SSTV state was open/active at 145.500 MHz with its stream running; each
ADS-B state streamed at 1090 MHz. Health uptime rose continuously and reset
reason remained 11, the earlier controlled restart. No new driver drops or
overruns were reported. This does not reproduce or clear the mixed incident.

Final state was explicitly restored to Home/IDLE. At 18:25:42 PDT, uptime was
924,559 ms; PSRAM free 2,767,260 bytes, DMA free 13,463, largest 12,800. The
logger remained ready with zero queued/pending/dropped records. Log:
`release-v4-sstv-adsb-navigation-isolation-20261026.log`. The command plan
completed with exit zero. No firmware source, build or flash was changed.

Next diagnostic milestone: record one controlled physical disconnect with
navigation, retaining serial output through any restart. A longer/explicitly
armed observation window is needed so waiting for the operator does not consume
the recording window, as happened with the five-cycle attempt.

## Controlled disconnect plus navigation

The operator performed one explicitly recorded V4 unplug/replug while a serial
recorder issued exactly one authenticated `RTL_UI OPEN ADSB` after detecting
the disconnect. The recorder was armed at 18:44:26 PDT; unlike the earlier
counter, its incident window starts on the disconnect. Pluto's TX mute was
freshly confirmed before arming. No PAIR, reset, flash or RF transmission was
performed by this diagnostic.

- Disconnect recorded at **18:45:05.104**; ADS-B navigation accepted at
  **18:45:05.208**, while the receiver was absent.
- New V4/profile 1 streamed at **1090 MHz**, about **2.048 MSPS**, bias and RTL
  AGC off. First observed stream was 13.777 seconds after disconnect, including
  operator unplug time and polling; this is not isolated enumeration latency.
- Health uptime remained monotonic, reset reason stayed 11, and the recorder
  observed no new reset or serial transport error during the three-minute
  incident window. The operator also reported the device stayed responsive.
- The final observed stream segment had increasing byte counters for 127.202
  seconds, with reported driver drops/overruns zero. Automatic Wi-Fi reconnect
  attempts paused/restarted the radio earlier in the capture. Zero per-stream
  drop counters must not be presented as uninterrupted reception across pauses.

**Preserved harness limitation:** the raw recorder result is INCOMPLETE, exit 1,
because it required manual gain 40.2 dB to survive reattachment. Inspection of
the unchanged pinned driver (`31a159df4f006a837d5041029bc6d1bc63520234`) verifies
that `clear_profile_runtime_state` clears bias and restores AUTO gain, then
`apply_profile_to_handle` applies the profile default. `rtl_profile.hpp` defines
AUTO for Blog V4, and the driver's existing profile test asserts that default.
Thus the observed AUTO state matches the implementation; preserving manual
gain was an incorrect expectation for this diagnostic, not an established new
firmware defect. No driver source was edited to obtain a pass.

The original recorder result was not rewritten. A separate raw-log analysis,
`python .local/maps/analyze_v4_disconnect_navigation_20261026.py`, passed checks
for the one disconnect, accepted navigation, monotonic uptime and sustained
streaming using the verified attach policy. Its result is
PASS_BOUNDED_RECOVERY_NO_RESET. One untimestamped serial fragment was skipped;
all scored snapshots have their required fields. This supports a bounded
successful recovery, **not a fix or clearance of the earlier watchdog**, a
five-cycle pass, packet decoding or an RF acceptance row.

Logs/artifacts beside invocation:

- `release-v4-disconnect-navigation-prepare-20261026.log`
- `release-v4-disconnect-navigation-20261026.log`
- `.local/maps/release-v4-disconnect-navigation-20261026.json` (raw INCOMPLETE)
- `.local/maps/release-v4-disconnect-navigation-assessment-20261026.json`

## Saved-data persistence and final state

After the incident and controlled crash-partition readback restart, serial
readback confirmed the six-row SSTV index retained its complete previous prefix.
Its SHA-256 remains
`d9680b977faacb69a0cbe6a704f3bf3bbb61f383385090b2e7091a41410506d4`.
The saved V4 Robot36 BMP is byte-identical to its pre-restart copy, SHA-256
`8833b478783b9552feec56bfcf7e0b90a7cd96bf409c13b892a8656b6be0d5cd`.
This is readable saved-data persistence, not an arbitrary power-cut test.

Command plan `release-v4-sstv-postrestart-storage-20261026.log` completed with
exit zero; the separate byte/prefix comparison passed. Final state is Home,
receiver IDLE, logger ready with queued/pending/dropped zero; uptime 2,465,844 ms,
reset reason 11. Pluto remains muted. No firmware, NVS or C6 change occurred;
COM39 was untouched. Next requested hardware is Blog V3c on COM17 for the same
conducted SSTV sequence. The earlier reset investigation remains open.

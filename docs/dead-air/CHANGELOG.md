# Dead Air Changelog

Changes to the Dead Air dashboard. Newest first. The design contract and the development handoff are in `DEVELOPMENT_HANDOFF.md`.

## 2026-10-22 - Dead Air dashboard merged into main (PR #203, merge commit `75353cec`)

Dead Air is a logbook of what the receiver measured. It does not detect ghosts, spirits or entities, and it shows no data it did not measure; interpretation is labelled separately from measurement.
The work was developed from 2026-10-18 and landed as one pull request (35 commits). This entry was written afterwards from the commit history and the PR description; the merge itself carried no changelog entry.

### Added

- **Dashboard** (`dead_air_dashboard`, `dead_air_model`, `dead_air_format`, `dead_air_palette`, `dead_air_info`): a native M5GFX dashboard with six tabs (Overview, Spectrum, Audio, Environment, Events, Review), opened from the normal dashboard list and `RTL_UI OPEN DEAD_AIR`.
  The panel is drawn off-screen and only changed tiles are pushed, so it does not flicker; an (i) button on every tab opens a plain-language page for that tab.
- **Receiver data and detector:** live receiver level, baseline and threshold, with an automatic RF-rise event; the detector is suspended while a sweep runs, because every hop is a new channel with no stable baseline.
  Readings from a disconnected or unsettled receiver are invalidated rather than shown. `RTL_DEAD_AIR STATUS` (no authentication, read-only) reports the state; `RTL_DEAD_AIR FLOOD <n>` (authenticated) is a bench command that logs manual marks.
- **Receiver controls:** only real demodulators are offered (NFM, WFM, AM, USB, LSB), each with its standard filter width; a generated sweep profile records its gaps. Leaving Dead Air returns Home to its previous frequency, band label and demodulator.
- **Spectrum and Audio tabs:** a live spectrum trace and a measured-spectrum waterfall; a demodulated-audio level meter and detector.
- **Evidence journal on the SD card:** a bounded event queue and a versioned JSONL session journal at `/orcsdr/dead_air/session_<id>.jsonl`, written by a low-priority worker (core 0). An event counts as saved only after the flush succeeds; a power cut leaves the file ending on a complete line.
  If the card is pulled the recorder reports "failed" and does not silently resume.
- **Clips:** event-linked clips of the receiver's demodulated audio (5 s before and 5 s after, WAV with a SHA-256 in `/orcsdr/dead_air/clips/`), a clip selector and replay on the Review tab on its own speaker channel at the receiver's volume, and a VERIFY button that checks the file on the card against the recorded hash.
  Microphone clips are opt-in (off by default, explicit start, a visible indicator, separate files, the speaker state recorded).
- **Host tests and CI:** ten host test suites (model, journal, renderer and touch, SD sink, sweep, palette, audio, clip planner, formatting, popup text) and the `Dead Air host tests` workflow.

### Fixed in the same change

- Mic clips recorded after a replay were constant-value silence (the shared I2S input stuck on one value); the input is restarted and such clips are refused.
- **General fix inside this change (required):** `0608e3b1` moves cold FT8 buffers (`Runtime g`, the decode chunk, two waterfall lines) to PSRAM with `EXT_RAM_BSS_ATTR`, freeing 23.9 KB of internal RAM. Dead Air's own statics would otherwise tip the boot-time 40 KB DMA pool over and abort start-up.
  A host stub, `tests/ft8_ui_stubs/esp_attr.h`, keeps `ft8_dashboard_tests` building.

### Validated on hardware (Tab5 on COM39, 2026-10-22)

- Boot with no `Could not reserve internal/DMA pool`; all six tabs; the (i) popups; MARK EVENT writing an event and a receiver clip; the journal fetched from the card (event, clip, clip_summary and session_end records); a clip fetched from the card hashing to the recorded SHA-256 and VERIFY agreeing;
  mic clips, replay and VERIFY on the fixed build; antenna unplug and replug (the level moved, but the replug rise stayed below the Standard threshold, so no event fired).

### Known limits and risks

- A real RF-rise event has not been captured on the final build; ENV III hardware (temperature, humidity, pressure) had not arrived.
- The SD worker shares the card with other tasks (UI loop, SD_GET, ACARS archive, FT8 SHOT), and `orcsdr_storage` has no lock and no unmount or remount: a pulled card needs a reboot before a new session can write.
- After leaving Dead Air for Home the band label first came back as BROWSE instead of SHORTWAVE (issue #206, fixed by `f3798608` before the merge).

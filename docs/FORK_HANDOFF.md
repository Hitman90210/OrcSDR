# OrcSDR Tab5 — fork handoff

Everything this fork (`Hitman90210/OrcSDR`) has changed on top of
`hardcoreerik/OrcSDR`, why, and how it was verified. Written to be picked up
cold: if you are new to this tree, read §1 and §2, then jump to whatever you
are touching.

Last updated: 2026-09-08. Head at time of writing: `d972d4a`, 31 non-merge
commits ahead of upstream, last upstream merge `b941f6f` (POCSAG feature).

---

## 1. What this is and how it is verified

Target: **M5Stack Tab5** — ESP32-P4 host with an ESP32-C6 Wi-Fi co-processor
over ESP-Hosted SDIO. ESP-IDF 5.5.4. The application is
`apps/orcsdr-tab5`, and `ui/main.cpp` is a ~13,500-line monolith that owns the
radio session, the streaming task, the serial CLI, and the generic radio
screen; each dedicated dashboard (FM, P25, ADS-B, LoRa, POCSAG, home) is its
own file.

**Nothing in this fork was accepted on a clean compile alone.** The working
loop for every change was:

```bash
# build
apps/orcsdr-tab5/tools/build-tab5-idf.ps1

# flash (COM3 on this machine)
python -m esptool --chip esp32p4 -p COM3 -b 460800 --before default_reset \
  --after hard_reset write_flash --flash_mode dio --flash_size 16MB --flash_freq 80m \
  0x2000 bootloader/bootloader.bin 0x8000 partition_table/partition-table.bin \
  0x10000 orcsdr_tab5.bin

# verify: 20 boot self-checks must all print _OK, no panic, memory sane
```

For UI work there is a second loop: the device renders any of its 46
documented screens on command and screenshots itself to SD, which is then
pulled back over serial.

```
UI_DOC_SHOW <screen-id> <live|demo>    # stage a screen
UI_CAPTURE <slug>                      # 1280x720 BMP to /orcsdr/screenshots/
SD_GET_BEGIN / SD_GET_CHUNK            # retrieve it
UI_DOC_EXIT                            # restore the previous screen
```

**Two traps worth knowing before you use it.** Opening a serial connection to
COM3 resets the P4 (native USB Serial/JTAG), so every new `serial.Serial()`
reboots the device. And an authenticated session drops after 5 s of silence
(`kSessionTimeoutMs`), which a long main-loop stall alone is enough to trip —
several early "the device is broken" results were actually expired sessions.
Sweep scripts here reconnect on failure for exactly that reason.

Screen ids and their permitted modes live in `kUiDocScreens` in `main.cpp`.
The `settings.*` screens and `fm.settings` are `demo`-only; asking for them in
`live` returns `unknown_screen_or_mode`.

---

## 2. Current state

- **Builds clean**, 45% of the app partition free.
- **20/20 boot self-checks pass**, no panics.
- Memory at `stage=ready`: ~82 KB internal free, ~42 KB DMA-capable,
  ~26.7 MB PSRAM. Watch the first number — see §7.
- Upstream's POCSAG pager decoder is merged and its self-check passes.

**The one thing that is still genuinely broken is the SDIO transport wedge
(§3).** Everything else below is either fixed or is a documented limitation.

---

## 3. The SDIO transport fault — the big one

This ate the largest share of the engagement, so it gets its own section.

### Symptom

Data & Maps downloads ("Could not fetch catalog manifest", "Could not fetch
catalog signature") fail, the device becomes unresponsive, and the serial log
floods:

```
sdmmc_send_cmd returned 0x107          (ESP_ERR_TIMEOUT)
eh_sdio: sdio_get_tx_buffer_num: err: 263
SDIO aggr: no slave credits (32 consecutive)
```

### What was ruled out, on hardware

| Hypothesis | Test | Result |
| --- | --- | --- |
| Wi-Fi power save | `esp_wifi_set_ps(WIFI_PS_NONE)` | **Not the cause.** Flood reproduced identically. Call kept (harmless, standard). |
| RTL-SDR / USB bus contention | Stop RTL streaming, then run a catalog check | **Not the cause of the wedge.** First test was invalid — a manual `RTL_STOP` fired before the device's own boot auto-start, and separately the 5 s session timeout dropped auth. Fixed the script; fault still reproduced. |
| SDIO clock too fast | Dropped `CONFIG_ESP_HOSTED_HOST_SDIO_CLK_KHZ` 10000 → 5000 | **Not the cause.** Identical ~30 s flood. Reverted to 10 MHz. |
| Our fork broke it | Built and flashed **unmodified upstream** in a worktree | **Reproduces identically on upstream.** `SDIO aggr: no slave credits (32-35 consecutive)` at boot, 8164 flood lines, catalog check never queued. |

### Conclusion

The fault is in Espressif's own `esp_hosted` SW_AGGR TX credit path: it detects
sustained failure but never escalates to the restart recovery its sibling
blocking path uses. `esp_hosted` 3.0.6 is already the latest release.
`CONFIG_ESP_HOSTED_HOST_TRANSPORT_RESTART_ON_FAILURE` is deliberately **off** —
turning it on risks a reboot loop.

### What was built around it

- **Bounded retry** in `catalog_sync.cpp`: 4 attempts with 1.5 s / 4 s / 8 s
  backoff, sized against the observed ~20 s wedges. Still bounded — no infinite
  retry, no vendor driver changes.
- **Manual "Reset Wi-Fi Link"** in Settings → Connectivity, and
  `RTL_WIFI_RESET_LINK` over serial. See §4 for the crash this caused first.
- **Transport health tracking** (`g_transport_healthy`) so the UI can stop
  poking a wedged link.
- **The honest recovery message.** When the link is wedged badly enough that
  the station will not even stop, the reset cannot run; the UI now says
  *"Link wedged - restart the device to recover"* rather than
  *"Wi-Fi link reset failed"*.
- **Sideloading as the real answer for big data packs** — see §6.

### Open lead, not yet chased

The user observed that unplugging the RTL-SDR made a download succeed. The
contention test above ruled USB out as the cause of *the wedge itself*, but not
as an aggravating factor. Worth a proper A/B. Note that the recent upstream
merge also bumped `esp_rtl_sdr` **v0.7.9 → v0.7.14**, which may change this
picture — retest on the current build before drawing conclusions.

---

## 4. Two mistakes worth inheriting

Both cost real hardware-recovery time. Do not repeat them.

### `netif_add` assert → panic → reboot loop

The first `reset_link()` called `esp_wifi_stop()` and then immediately
`esp_hosted_deinit()`. `STA_STOP` never arrived, so the C6's Wi-Fi never
actually stopped; two `STA_START` events with no intervening `STA_STOP` hit
`assert failed: netif_add (netif already added)`.

Root cause, confirmed by reading ESP-IDF's `esp_netif_lwip.c`:
`esp_netif_start_api()` → `esp_netif_lwip_add()` calls `netif_add()`
**unconditionally, with no already-added guard**. Only `esp_netif_stop_api()`
removes it.

The fix in `wifi_service.cpp` waits up to 3 s for a real `STA_STOP` before
touching the transport, and **aborts safely** if it does not arrive rather than
risking the same crash. `reset_link()` also deliberately does *not* call
`stop()`/`start()` — those re-register RPC event handlers.

Related correction: an explicit `esp_wifi_start()` in that path looked
redundant. Reading `eh_host_core_bringup_mcu.c` first showed that transport
bring-up does **not** restore Wi-Fi mode/state, so it is necessary. Check the
vendor source before deleting anything that looks redundant in this area.

### Internal RAM, not stack, is the scarce resource

Two 16-element arrays (`wifi_ap_record_t records[16]` in `scan_results()`,
`ScanResult scan[16]` in `poll_wifi()`) were changed from stack to `static` as
"stack hardening". Result:

```
E main_task: Could not reserve internal/DMA pool (error 0x101)
abort() → continuous reboot
```

Free internal RAM went 43 KB → 40 KB and `app_main` could no longer reserve its
40 KB internal/DMA pool. **Both were reverted to stack allocations** and carry
comments saying why. `EXT_RAM_BSS_ATTR` is available when a static really is
needed — put it in PSRAM, not internal.

Corollary: `RTL_DRAM_BUDGET` lines in the boot log are the canary. If
`intern_free` at `stage=ready` drops toward 40 KB, something new is eating
internal RAM.

---

## 5. Fixes by area

### 5.1 Rendering — the shared-global-state class of bug

M5GFX keeps several pieces of drawing state **globally on the display object**.
Three separate bugs in this tree came from that, and it is the first thing to
suspect for any "it draws wrong only sometimes" report.

**Scroll rect (the worst).** `M5.Display.scroll()` scrolls whatever rect was
last set *anywhere*. The generic radio screen in `main.cpp` only ever set it
when the nav panel opened — so for the rest of the time it was scrolling
against whichever dashboard had touched the rect last. FM, P25 and LoRa each
set theirs in a *different function* from where they scroll, with the same
hazard.

Fixed by removing the dependency entirely: **`ui/waterfall_view.{hpp,cpp}`** is
a PSRAM ring buffer pushed through `writeImage` — the path every other element
on those screens already renders through. All four waterfalls use it. Rows are
packed at the active width so a narrowed plot (CB and LoRa use one
permanently) is still two contiguous blits rather than ~250 per-row calls.

`rf_visualizer.cpp` and `rf_lab.cpp` scroll *sprites*, not the panel. That is
fine and was left alone.

**Font.** ADS-B and POCSAG select DejaVu fonts and the shared header inherited
whatever was left active, rendering the "VIS" icon oversized into its
neighbours. Both dashboards reset the font on `leave()`, and the shared header
resets defensively. There is a regression check (`home_font_ok`) for it.

**Text datum / colour.** Same class; watch for it.

### 5.2 Radio behaviour

- **FM SEEK did not seek.** It hopped between *saved presets*, so it only ever
  visited the handful of frequencies a previous band scan had stored — exactly
  the reported "goes through a limited few select lower frequencies". It now
  sweeps the band in 800 kHz scope windows from the current dial using the
  existing `scan::Engine`, stops on the first station past it, and wraps at the
  band edges (`fm_seek_wrap` folds the uint32 arithmetic back into the band).
  Early-stop is signalled out of the measure callback and acted on *after*
  `service()` returns, so the engine is never re-entered.
  **Not yet confirmed against live RF** — no antenna on the bench.
- **NOAA weather was pinned to 162.400.** `rtl_clamp_frequency` returned
  `kRtlWxHz` for the whole band, STEP was a no-op, and
  `request_hot_retune_for` refused the `wx` band outright — six of the seven
  channels were unreachable. The band is channelized now (same shape as CB) and
  the home dashboard's span/step strip becomes a **WX1–WX7 picker** on that
  screen. Span, step and filter say nothing about a fixed 25 kHz NFM channel.
- **Mute did not mute.** `apply_speaker_volume()` — the chokepoint every
  volume path funnels through — ignored mute state, so muting only stopped
  *new* samples being queued while the amp stayed at full volume. Enforced at
  the chokepoint.

### 5.3 Wi-Fi

- **UI lag root-caused.** `wifi::rssi()` made a blocking RPC (msg id 294,
  `Req_WifiStaGetApInfo`) from the render path at 2 Hz. Every 5 s main-loop
  stall paired with a 294 timeout. The wedge did not freeze the UI — *this poll
  blocking on it* did. `rssi()` now caches, refreshing every 5 s when healthy
  and every 30 s while wedged, and uses a successful probe to clear the wedged
  flag.
- **Auto-reconnect never ran for hand-made connections.** It required
  `settings_wifi_start_at_boot`, so a link the user brought up by hand simply
  stayed down after a drop — which is what "it just disconnects" looks like
  from outside. A hand-made connection now counts for the rest of the session
  (`wifi_session_wants_connection`), with bounded 15→60 s backoff. An explicit
  disconnect still suppresses it.
- **Failures name a reason.** `last_disconnect_reason()` /
  `disconnect_reason_text()` turn a bare "Connection failed" into
  "Failed: wrong password" vs "Failed: beacon timeout" — very different
  problems that were previously indistinguishable.

### 5.4 Crashes fixed

- **`ui_doc_render()` stack overflow.** One ~200-line function handled every
  screen type in one if/else chain, so the compiler reserved the *union* of all
  branches' locals — 6000 bytes — even though one branch runs per call. Split
  into `[[gnu::noinline]]` helpers; the parent dropped to 48 bytes. Diagnosed
  with `gcc -fstack-usage`, not trial and error. Two earlier attempts to just
  raise the stack size (20480, then 14336) left the device not booting at all.
- **`catalog_sync` stack overflow.** Three stacked instances of the same
  anti-pattern — `Pack[16]` (~820 bytes each) as a *local* array against a
  12288-byte task stack. `worker()` held all three operations' locals in one
  15344-byte frame; `refresh_installed()` copied the whole 13248-byte global
  onto the stack; `parse_manifest()` had its own 14976 bytes. All three moved
  to the heap / split into noinline branches. Final frames measured, not
  guessed: 32 B / 176 B / 1888 B / 1776 B / 256 B.
- **SD never powered its slot.** `mount_tab5_sd()` did not drive GPIO45 before
  mounting, unlike the splash screen's own copy of that logic, so the first
  boot mount was guaranteed to fail. Power-gate centralised; the real
  `esp_err_t` is surfaced instead of a generic "missing_or_fail".

### 5.5 Self-checks were lying

Every `self_check()` line in `setup()` printed `_OK` **unconditionally right
after** `_FAIL`, masking real failures. Rewritten as single conditionals.
This masked a genuine failure: `kUiDocScreens` was missing
`settings.firmware-updates`, and the screen-id → `Section` lookup mapped **by
array index**, so every section after `connectivity` was silently off by one
and `Section::system` was unreachable. Fixed, with a `static_assert` so the
drift fails to compile next time.

> **Note for the next upstream merge:** upstream re-introduced this exact bug
> in its POCSAG branch (`if (!check()) println(FAIL); println(OK);`). The merge
> `b941f6f` deliberately keeps our ternary form. Do not take theirs.

### 5.6 UI — found by a full 46-screen capture sweep

The screens were swept twice, before and after changes. Highlights:

| Screen | Defect |
| --- | --- |
| FM Station/RDS | One 260px-tall rectangle cleared all three status cards, wiping the PILOT/DECODER STATUS captions and borders every refresh — the reported "looks like it's hiding something". |
| Home / radio family | The card at (930, 484) repeated the stepper below it verbatim: "STEP 12.5 kHz" twice on free-tuned bands, "CHANNEL CH 19" twice on CB. Now FILTER / "19 / 40"; the freed slot widens SIGNAL. |
| Home header | Title fell back to "HOME" for any frequency in no named band (`Id::utilities` has no registry entry) — browsing 146.520 MHz showed a tuner under the word HOME. |
| ADS-B radar | "N" drawn at y=2 and "S" at y=396 in a 390px sprite; both clipped. All four compass points moved inside the outer ring. |
| ADS-B data card | "NO NEARBY PRESET / MANUAL START / UNAVAILABLE" never said which of three preconditions was missing. Now names the first unmet one. |
| LoRa | The decoder state (DECODE/PHY/KEY) was drawn at x=1050 y=44/73 — *under* the header icons drawn afterwards, so it was completely invisible. Restacked to x=866. |
| LoRa RF health | Card widths summed to 1270 and the row ended at 1334, off-screen. Rebalanced to end at 1248. |
| Settings | CLOSE overlapped the shared mute icon (116px button in ~79px of gap); its hit-test moved too, which would otherwise have silently broken tapping. |
| ADS-B | "SET RECEIVER LOCATION" button had **no hit-test at all** — tapping did nothing. |
| DEMO badge | Sat at (572, 8), directly on the home header's status panel (starts at x=595), covering the Wi-Fi cell and IP in the very captures it exists to mark. Moved to a bottom strip. |
| Header icons | Spacing and height inconsistent (58×58 @ y=8 vs 54×54 @ y=12). Unified. |

Also: several redraw-only-on-full-entry bugs (mute icon, VIS icon) where
toggling state looked stuck until you navigated away and back.

### 5.7 Honesty fixes

Cases where the UI stated something untrue:

- **Data & Maps** queued a worker for packs the published catalog does not
  carry, said "Data operation queued", then silently did nothing. The published
  catalog has **only** `faa_aircraft`, `faa_aviation` and `lane_county_map`;
  `noaa_weather` and `fcc_broadcast` are reserved ids with no artifacts. The
  button now reads UNAVAILABLE and does not arm — but only *after* a catalog
  check, since before one the honest state is "we have not looked yet".
- **`atc::nearest()` was unbounded**, so with a national index a receiver in
  Oregon would be offered a Florida tower and told it was nearby. Capped at
  ~100 nmi, with longitude weighted by cos(latitude).
- The ADS-B card claimed "LANE COUNTY READY" for whatever map was loaded.

### 5.8 Dead code and build

- 705 lines of `RTL_USE_LEGACY_USB` blocks removed. The resulting binary was
  **byte-identical in size**, confirming pure dead-code removal.
- CI: `actions/checkout` → v7.0.1 (Node 20 deprecation); ESP-IDF environment
  sourced before `idf.py`; component-hash mismatch under CRLF checkout fixed.

---

## 6. Making the device local to the user

The device's location-dependent features all assumed the upstream author's
county. Full instructions are in **`docs/LOCAL_SETUP.md`**; the short version:

- **Offline maps** — `offline_map` now loads `/orcsdr/data/local_map.idx` in
  preference to the packaged Lane County pack.
  **`apps/orcsdr-tab5/tools/build_orcmap.py`** builds that file for any
  bounding box straight from the Overpass API, with per-class segment budgets
  (roads / water / airports) so a coastal map keeps its coastline instead of
  spending all 640 segments on roads. Verified live for Seattle:
  437 road / 200 water / 3 airport / 4 labels.
- **P25** — already supported user profiles via SD import; it was just
  undiscoverable and undocumented. The empty state now explains it, and
  `LOCAL_SETUP.md` carries a worked example **checked line-by-line against
  `p25_config.cpp`'s parser**: decimal only (RadioReference prints hex),
  `control_channel_hz` in **Hz** not MHz, `talkgroup = id, alias` with the
  comma required.
- **Data packs** — direct download links, exact destination filenames (they
  differ from the download names, which is easy to get wrong), and the SD-card
  route, which is far more reliable than a 34 MB download over the C6 link.
  Surfaced on the Data & Maps screen itself and in the README.

---

## 7. Gotchas

- **Internal RAM budget** — §4. Watch `RTL_DRAM_BUDGET intern_free`.
- **Shared display state** — §5.1. Scroll rect, font, text datum, clip rect.
- **`active_scan` is streaming-task-only.** Do not read it from touch or
  serial handlers; use an atomic mirror (`fm_seek_active`,
  `rtl_fm_preset_scan_active`, `p25_survey_active`).
- **Serial resets the device.** Every connection. Budget ~12 s before the
  device is ready.
- **Settings' `text()` floors size at 2** (`kSettingsMinTextSize`), so a size-1
  request still renders at 12 px per character. A footer written for size 1
  overflowed the screen by ~240 px because of this.
- **Default font advances 6 × size px per character.** `adsb_dashboard` and
  `pocsag_dashboard` override to proportional DejaVu, so character-count width
  maths does not apply there.
- **Demo mode has a real geometry mismatch.** `draw_documentation_spectrum()`
  paints at `main.cpp`'s radio geometry (x 65, y 316, 1150×250) with a
  deliberate per-row shear, but `*.radio` screens in demo mode stage the *home*
  layout (plot at x 330, waterfall y 298..456). The result looks like a badly
  corrupted waterfall smeared across the sidebar. **It is staged artwork drawn
  at the wrong coordinates, not a rendering fault**, and it does not affect
  live screens. Not yet fixed.

---

## 8. Still open

1. **SDIO transport wedge** — upstream `esp_hosted` defect, reproduces on
   unmodified upstream. Mitigated, not fixed. Restart is the reliable recovery.
2. **RTL-SDR contention A/B** — not done; retest on the v0.7.14 driver.
3. **FM SEEK not confirmed against live RF.**
4. **`noaa_weather` / `fcc_broadcast` packs** cannot be published from this
   fork — the catalog is signed with the upstream author's P-256 key and the
   firmware embeds only the matching public key.
5. **Demo-mode spectrum geometry mismatch** — §7.
6. **POCSAG polish** — `MESSAGE DETAILS` has no empty-state placeholder while
   its sibling panel does; the SIGNAL tab's "CORRECTED 0 bits=0" breaks the
   right-aligned value column every other row keeps.

---

## 9. Map of the interesting files

| File | What lives there |
| --- | --- |
| `ui/main.cpp` | Radio session, streaming task, serial CLI, generic radio screen, scan drivers, snapshot builders. Everything not in a dashboard. |
| `ui/waterfall_view.{hpp,cpp}` | Ring-buffer waterfall. Added by this fork. |
| `ui/wifi_service.{hpp,cpp}` | Station lifecycle, `reset_link()`, transport health, disconnect reasons. |
| `ui/catalog_sync.{hpp,cpp}` | Signed data catalog: fetch, verify, install, remove. |
| `ui/home_dashboard.cpp` | Home *and* every band without a dedicated dashboard (CB, weather, shortwave, browse). |
| `ui/p25_config.{hpp,cpp}` | P25 profile parse/validate/store. Strict parser — read it before writing a profile. |
| `ui/scan_engine.{hpp,cpp}` | Generic non-blocking retune/measure sweep with a host self-check. |
| `ui/offline_map.{hpp,cpp}` | ORCMAP1 loader. User map wins over the packaged one. |
| `tools/build_orcmap.py` | Build a map for your area. Added by this fork. |
| `docs/LOCAL_SETUP.md` | User-facing: data packs, maps, P25, location. Added by this fork. |

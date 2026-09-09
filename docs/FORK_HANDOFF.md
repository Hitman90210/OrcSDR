# OrcSDR Tab5 — fork handoff

Everything this fork (`Hitman90210/OrcSDR`) has changed on top of
`hardcoreerik/OrcSDR`, why, and how it was verified. Written to be picked up
cold: if you are new to this tree, read §1 and §2, then jump to whatever you
are touching.

Last updated: 2026-09-09. The fork now includes upstream `ff8a5ee`
(`v0.2.0-beta.5`) plus the fork-specific integration fixes described below.

---

## 1. What this is and how it is verified

Target: **M5Stack Tab5** — ESP32-P4 host with an ESP32-C6 Wi-Fi co-processor
over ESP-Hosted SDIO. ESP-IDF 5.5.4. The application is
`apps/orcsdr-tab5`, and `ui/main.cpp` is still a ~14,900-line monolith that owns the
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

# verify: 22 boot self-checks must all print _OK, no panic, memory sane
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

**Three traps worth knowing before you use it.** Opening a serial connection
to COM3 *can* reset the P4 (native USB Serial/JTAG) — but `tools/help_media.py`
sets `dtr = False; rts = False` **before** opening precisely to avoid that, and
`copy_to_tab5_sd.ps1` does the same. So a script built on those helpers does
**not** reboot the device, and anything cached at boot (the offline map, the
ATC preset) will still hold pre-change state. To force a real reboot, toggle
RTS yourself:

```python
s = serial.Serial("COM3", 115200, timeout=1)
s.setDTR(False); s.setRTS(True); time.sleep(0.1); s.setRTS(False)
```

This cost an hour: a map uploaded to the SD card kept reading as "not loaded"
because `offline_map::load()` only runs at boot and the device had never
actually rebooted.

And an authenticated session drops after 5 s of silence
(`kSessionTimeoutMs`), which a long main-loop stall alone is enough to trip —
several early "the device is broken" results were actually expired sessions.
Sweep scripts here reconnect on failure for exactly that reason.

Screen ids and their permitted modes live in `kUiDocScreens` in `main.cpp`.
The `settings.*` screens and `fm.settings` are `demo`-only; asking for them in
`live` returns `unknown_screen_or_mode`.

---

## 2. Current state

- **Builds clean**, 44% of the app partition free.
- **22/22 boot self-checks pass**, no panics.
- Memory at `stage=ready`: ~82 KB internal free, ~42 KB DMA-capable,
  ~26.7 MB PSRAM. Watch the first number — see §7.
- Upstream's POCSAG pager decoder is merged and its self-check passes.
- **ADS-B verified end to end on air (2026-09-09).** 1090 MHz, live traffic:
  receiver at 2,047,945 sps with zero overruns, drops or IQ queue drops;
  78,632 preambles, 11,254 DF17, 1,074 CRC-valid messages, 24 aircraft tracked.
  Altitudes 5,775-23,850 ft and ground speeds 263-441 kts are all plausible.
  The 34 MB FAA aircraft pack resolved **31 of 32** ICAO addresses to tail
  numbers (A8996B to N653RW, A9F461 to N740UW, and so on), so the database
  lookup path works, not just the decoder.

  Callsigns decode correctly (`ASH6130`, `UAL21`). A first pass reported none
  and that was a measurement error worth recording: `RTL_ADSB_FRAME` is emitted
  only at TRACE, and identification messages are type codes 1-4, which are just
  **2 of 211** frames -- about 1% of the mix against 156 TC 0, 25 TC 19
  velocity and 21 TC 11 position. A short sample containing no callsign is the
  expected outcome, not a fault. Assert on the RTL_ADSB_STATUS counters, which
  are authoritative and available at DEBUG, and sample frames separately.

- Upstream beta.5 regional LoRa plans, retained traffic scrolling, packet-log
  export, non-blocking console writes, and deferred Wi-Fi startup are merged.
- The exact 2026-09-09 app image passed the authenticated UI regression and
  the paused-connect, scan, and explicit live-load Wi-Fi coexistence diagnostic
  on COM3 with RTL-SDR 0.7.14 and zero USB, IQ, or audio drops.

**The remaining high-risk area is still the shared SDIO/USB startup path
(§3 and §3b).** Normal Wi-Fi work is protected by the radio-pause window, and
beta.5 defers Hosted startup out of `setup()`. Battery-only boot of this exact
merged image has not yet been rerun, so the older watchpoint workaround remains
enabled and §3b should still be read before changing Wi-Fi bringup or FreeRTOS
configuration. Everything else below is either fixed or documented.

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
| RTL-SDR / USB bus contention | See §3a — this turned out to be **the cause**. Two earlier attempts to test it were both invalid. | **CONFIRMED CAUSE.** |
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

### 3a. The RTL-SDR is the trigger — confirmed by A/B

The user's own observation ("I unplugged the sdr thing and it worked") was
right, and two earlier attempts of mine to test it were both invalid: the
first because a manual `RTL_STOP` fired before the device's own boot
auto-start, the second because every catalog check in it returned
`auth_required` — the 5 s session had expired during the read loop, so the
check never ran at all. Neither measured anything.

The valid experiment uses **Wi-Fi association** as the probe (~25 s per trial
instead of 90) with a keepalive every 2 s:

| Condition | Trials | Connected | SDIO errors |
| --- | --- | --- | --- |
| Dongle unplugged | 15 | **14** | **0** |
| Dongle plugged back in, streaming | 15 | **0** | 19,963 |
| Dongle plugged in, **stream stopped** | 15 | **15** | **0** |
| Dongle streaming, **after the fix below** | 10 | **10** | **0** |

The first two ran in one power-on session with no power cycle between them, so
the only change was the dongle. 18 of 20 attempts in the plugged run failed at
`connect_start_failed`: the connect could not even begin.

The third row is the one that matters. **It is the USB DMA traffic, not the
dongle's presence** — leaving the dongle physically attached but stopping the
stream restores association completely. That means a safe software fix exists
and the risky DMA-placement config change is unnecessary.

**The fix:** `start_wifi_connection(bool pause_radio = true)`. The pause
machinery (`pause_radio_for_io` / `resume_radio_after_io`) already existed and
the catalog paths already used it; the parameter simply defaulted to `false`,
so every Wi-Fi connect ran against a streaming dongle. It stops the stream and
speaker, waits for the stream to actually stop (5 s deadline), and restores
both on every exit path including the 15 s connect timeout in `poll_wifi()`.
Cost is a ~3 s audio gap on connect.

Timing shows the contention even when it works: 3076–3088 ms with the stream
running vs 3025–3031 ms without.

**Likely mechanism, not yet proven:** both DMA paths sit in PSRAM —
`CONFIG_EH_HOST_PORT_DMA_PREFER_SPIRAM` and
`CONFIG_USB_HOST_DWC_DMA_CAP_MEMORY_IN_PSRAM` are both on (the boot log prints
`hosted_dma_psram=1 usb_dma_psram=1`). An RTL-SDR at 2.048 MSPS is ~4 MB/s of
USB DMA into PSRAM competing with the SDIO transport's own PSRAM DMA, which
would explain `sdmmc_send_cmd returned 0x107` (bus timeout).

Moving the hosted SDIO DMA to internal RAM
(`CONFIG_EH_HOST_PORT_DMA_PREFER_SPIRAM=n`) would attack this directly and
might let reception continue during downloads. It is **not** needed for
correctness now that the pause is in place, and it is the exact class of change
that boot-looped this device before — internal RAM is the scarce resource (§4).
Only worth attempting with the DRAM budget measured first.

An earlier confound worth remembering: the plugged runs were originally done
*before* a full power cycle and the unplugged ones after, so "power cycle"
looked like it might be the real variable. Re-plugging the dongle in the same
session with no power cycle is what settled it.

### 3b. Battery boot loop — masked, not fixed

**Symptom.** The device boot-loops to a blue screen when powered from the
battery, and runs normally the moment USB-C is plugged in. Independent of the
RTL-SDR dongle. First observed 2026-09-08, on the first battery boot anyone had
ever tried — so there is no evidence it ever worked, and no reason to think
this fork introduced it.

**Evidence.** Battery boots leave a coredump (`CONFIG_ESP_COREDUMP_ENABLE_TO_FLASH`
is on, `coredump` partition at `0x410000`). Read it back over USB-C afterwards:

```powershell
python -m esp_coredump --port COM3 --chip esp32p4 info_corefile `
  build-native-hosted3\orcsdr_tab5.elf
```

Three dumps, all the same panic, all in different places:

| Dump | Crashing frame | Allocation |
| --- | --- | --- |
| 1 | main task inside `eh_host_core_bringup` → `reconfigure_attempt` | — |
| 2 | hosted **RX**: `sdio_process_rx_task` → `rx_upcall` | `malloc(8)` |
| 3 | hosted **TX**: `tx_worker` → `eh_host_feat_rpc_ext_v2_pack` | `malloc(10)` |

```
Panic reason: assert failed: block_locate_free tlsf_control_functions.h:618
              (block_size(block) >= *size)
```

Eight- and ten-byte allocations corrupt nothing. They are the first code to
touch a heap that was **already** broken, which is why the victim differs every
time. In dump 2 the frame pointer handed to `rx_upcall` (`0x4ff339d8`) was
unreadable and sat among the FreeRTOS TCB addresses.

**What was ruled out, on hardware.** Every row is a real build and a real
battery power-on:

| Build | Heap poisoning | End-of-stack watchpoint | Battery |
| --- | --- | --- | --- |
| Merged head (GMRS + scanner + upstream trampoline patch) | off | off | **fail** |
| Upstream trampoline patch reverted (bisect) | off | off | **fail** |
| C6 post-power-on delay 200 ms → 1000 ms | off | off | **fail** |
| Light poisoning + watchpoint | light | on | boots |
| Poisoning removed again, watchpoint kept | off | on | boots |

- **Upstream's `vTaskDeleteWithCaps` fix (#66) is not the cause.** Backing it
  out reproduced the loop with a byte-identical panic. It is merged.
- **Not a C6 bringup timing problem.** A 5× longer delay changed nothing. The
  delay is kept anyway — 200 ms was never a comfortable margin for a rail rise
  plus a C6 firmware boot.
- **Not a stack overflow.** `CONFIG_FREERTOS_WATCHPOINT_END_OF_STACK` never
  fired, and it traps a stack overrun regardless of heap layout.
- **Not caught by poisoning.** Light poisoning never reported a bad canary; all
  it did was shift the heap layout, and the loop stopped.

**What it looks like.** Layout-sensitive *and* timing-sensitive, with an
asynchronous corruptor that leaves a different victim each run: an SDIO/DMA
write landing in memory that has already been freed, inside Espressif's
`esp_hosted` transport. Every frame in every dump is in vendor code. Note §3
documents a separate, independently confirmed fault in that same SDIO path.

**Why the tree is the way it is.** `CONFIG_FREERTOS_WATCHPOINT_END_OF_STACK=y`
is the only thing both working builds share and no failing build had. It does
not repair anything — it arms a hardware watchpoint and adds work to every
context switch, and that timing shift moves the race out of the way. It is kept
because the device otherwise will not boot unplugged, and because a
stack-overflow tripwire earns its keep anyway. **Treat this as mitigation. The
fault is still in there.**

`heap_guard()` in `main.cpp` is the tripwire for the recurrence: seven boot
checkpoints (`m5_begin`, `graphics`, `self_checks`, `pre_wifi`, `c6_power`,
`pre_hosted_start`, `post_hosted_start`) run `heap_caps_check_integrity_all()`
and, on failure, `esp_system_abort()` with the checkpoint's name — which lands
in the coredump's "Panic reason". On USB-C all seven print `ORC_HEAP_OK`. If a
battery boot loop returns, the dump names the stage instead of leaving you
where this started.

**If you pick this up again.** Un-mask it first (drop the watchpoint config),
confirm it reproduces, then get a channel out of a battery boot — SD-card boot
logging, or RTC `NOINIT` memory read back over USB-C — because serial is gone
the moment the cable is. Then instrument the hosted SDIO buffer lifecycle for a
free followed by a late DMA completion.

---

### 3c. The LoRa capture trigger keyed on the wrong thing

Measured 2026-09-09. A 60-minute watch on the US LongFast default slot (20,
906.875 MHz) ran **345** decode cycles and reported `preambles=0` on every one.
No traffic -- but characterising the monitor mattered more than the silence.

**The fault.** `lora_iq_offer()` armed a capture when `rtl_signal_dbfs` rose
9 dB above a tracked noise floor. That value is *wideband* power across the
whole 960 kHz the tuner hears, not the LoRa channel. On 915 MHz ISM the window
is full of other traffic, so a garage remote or a tyre sensor three hundred
kilohertz away armed a capture exactly as well as a mesh packet. Over 180 s
that produced 24 captures, 24 decodes and **zero** preambles or header
failures: a 100% false-trigger rate, each costing 1.8-3.1 s of decode during
which the detector is not evaluated at all.

This is the same mistake the channel scanner had before 5.7a, and it has the
same shape of answer: ask what is on the *channel*, not what is in the window.

**The fix.** `lora_channel_excess_db()` sums four consecutive IQ samples --
a cheap decimating boxcar whose passband is roughly the LoRa channel (first
null at 240 kHz, -3 dB near 106 kHz, against a 250 kHz channel) -- and compares
that filtered power to the raw wideband power. Correlated in-channel energy
survives the sum; energy spread across the window does not. A capture now needs
both the old level rise and `kLoraChannelExcessMinDb` of concentration.

The scale runs from about -6 dB (flat noise) to 0 dB (all energy in-channel),
and *below* -6 dB when the energy is mostly outside -- which is the interferer
case. Modelled against known inputs, and measured on air:

| Case | Channel excess |
| --- | --- |
| Out-of-channel interferer, 300-400 kHz off, SNR 10-20 dB | -11 to -13 dB |
| Noise only | -5.8 dB modelled, -5.5 dB measured (median of 172) |
| In-channel LoRa chirp, SNR 3 dB | -2.9 dB |
| In-channel LoRa chirp, SNR 20 dB | -1.2 dB |

Threshold set to **-4.0 dB**: above the live noise bulk (p95 -5.3) and still
below the weakest chirp modelled, so weak packets are not discarded.

**Result, measured over 180 s each way.** The detector is evaluated on every IQ
block, so the only blind window is while the decoder is busy:

| | Decodes | Mean decode | Blind | Watching |
| --- | --- | --- | --- | --- |
| Before | 24 | 2348 ms | 56.4 s | 69% |
| After | 3 | 3011 ms | 9.0 s | **95%** |

False triggers fell 24 -> 6 per 180 s in one run and 24 -> 3 in another. Not
elimination: the remainder are excursions where in-channel energy genuinely
rose, which the metric is right to pass on. The trigger is demonstrably still
live (`rf_events` still increments) -- worth stating because an earlier value
of this threshold was set from a misreading of the scale, sat above the maximum
the metric can produce, and silently disabled the trigger entirely. Zero
captures looked like perfect selectivity and was actually a dead detector.

**Two measurement traps here.** `RTL_LORA_ENERGY` is TRACE-gated, so a pass at
NORMAL reported zero energy triggers -- the same trap the ADS-B callsign count
fell into. And "air time captured" is *capture* time, not monitoring time:
fewer false triggers lower it while improving coverage, so it reads backwards.
Use decode time as the blind window instead. `RTL_LORA_TRIGGER_STATUS` reports
level, noise, trigger, live channel excess and the threshold without needing
TRACE.

**What a quiet result means.** Even at 95% coverage this is a receive-only
monitor on one slot; absence of traffic is evidence about this location and
this slot, not about the mesh generally.

---
### 3d. The LoRa decoder cannot decode 500 kHz Meshtastic presets

Meshtastic moved the US stock preset off **LongFast** because its 250 kHz
bandwidth is not US-compliant: FCC 15.247 expects at least 500 kHz for a
digitally-modulated system in the 902-928 MHz band. Two presets are 500 kHz
wide, and this fork's native decoder can decode neither.

| Preset | Bandwidth | SF | Coding rate |
| --- | --- | --- | --- |
| Long Fast (old US stock) | 250 kHz | 11 | 4/5 |
| **Long Turbo** (US replacement) | **500 kHz** | 11 | 4/8 |
| Short Turbo | **500 kHz** | 7 | 4/5 |

Preset names are easy to confuse here -- Long Turbo and Short Turbo are both
500 kHz and differ only in spreading factor, and the frequency alone cannot
tell them apart. What the frequency *does* pin down is the bandwidth, which
is the part that matters below.

**The grids differ, so a slot number is meaningless without the bandwidth.**
From `RadioInterface.cpp`:

```
freq        = freqStart + (bw / 2000) + (channel_num * bw / 1000)
numChannels = floor((freqEnd - freqStart) / (spacing + bw / 1000))
channel_num = (config.channel_num ? config.channel_num - 1 : hash(channelName))
              % numChannels
```

For the US (902.0-928.0 MHz, spacing 0):

| Slot shown | at 250 kHz | at 500 kHz |
| --- | --- | --- |
| 14 | 905.375 MHz | **908.750 MHz** |

A node reporting "Frequency slot 0" is on *auto*: Meshtastic hashes the
channel name to pick the slot. Read the frequency the node displays, never
the slot number alone. An observed 908.750 MHz is only reachable on the
500 kHz grid, which is how the bandwidth was identified here.

**Why it cannot decode.** `lora_native_decoder.cpp`:

```c
constexpr uint32_t kDecodeRate = 500000;   // exactly 2 samples per chip at 250 kHz

bool configure_chirp(uint8_t sf, uint32_t bandwidth_hz) {
  if (sf < kMinSf || sf > kMaxSf || bandwidth_hz != 250000 || ...)
    return false;
```

The decoder is built around two samples per chip at a fixed 250 kHz, so any
other bandwidth is refused before a sample is examined. `decode()` returns 0
and `preambles` stays 0 however strong the signal is.

**Confirmed on air, 2026-09-09.** A Meshtastic node feet away on 908.750 MHz,
receiver tuned to match:

- At automatic gain the bursts arrived at **-2.0 to -4.4 dBFS** -- essentially
  full scale, against -19 to -25 dBFS for the quiet band. An 8-bit ADC clips
  there, so gain was dropped to manual 7.7 dB.
- With sane levels the energy trigger fired repeatedly at a channel excess of
  **-0.8 to -1.5 dB**, the top of the scale and exactly what 3c's model
  predicts for a strong in-channel chirp. **That is the accept half of the
  trigger proved on a real transmission**, which noise and interferer tests
  could not establish.
- Every decode still returned `preambles=0`, at both SF 7 and the parameters
  above, because the bandwidth check rejects the capture outright.

So the antenna, tuner, trigger and capture path all work end to end. The
decoder alone refuses the parameters.

**What supporting 500 kHz would take.** Not a one-line relaxation:

1. `kDecodeRate` must become `2 x bandwidth_hz` -- 1 MHz for a 500 kHz
   channel, not a fixed 500 kHz.
2. The LoRa capture runs at 960 kHz, which cannot supply two samples per chip
   at 1 MHz. The driver already sustains 2,047,945 sps for ADS-B so a faster
   capture is available, but it doubles the IQ buffer for the same duration,
   and internal RAM is this project's binding constraint.
3. `configure_chirp()` and `interpolated_iq()` must take the bandwidth rather
   than assume it, with FFT scratch sizing following.
4. Long Turbo also uses coding rate 4/8 where the decoder has only been
   exercised on 4/5; worth confirming that path.

**Meanwhile:** `RTL_LORA_MODEM <sf> <bw_hz>` was added, because spreading
factor and bandwidth were touch-only and a receiver could not be matched to a
transmitter over serial at all. 250 kHz presets still decode; 500 kHz tunes
and triggers correctly but will not decode until the above is done.

**Two populations now exist** -- nodes on old LongFast and nodes on the newer
500 kHz stock preset -- and which one a node belongs to depends on when its
firmware was flashed. Anyone still on LongFast is unaffected by all of this.

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

  **Confirmed on air**, and getting there exposed three real defects that a
  clean compile could never have caught:

  1. *The measurement was stale.* `rtl_scope_peak_level` is computed inside
     `draw_spectrum()`, which returns early when the current tab shows no
     spectrum. On the FM LISTEN tab every window of a sweep therefore read an
     identical frozen value. `spectrum_measurement_wanted()` now lets the DSP
     run while a scan or seek is active. **This affected the pre-existing FM
     preset scan too** — it reads the same value.
  2. *The threshold was absolute.* The former `kFmPresetMinDbfs` (-70 dBFS) cannot
     separate a station from a noise floor that moves with gain and antenna,
     so on a real antenna every window passed and the seek stopped on the
     first one it looked at — including 76.8 MHz. Replaced with an in-window
     SNR (peak minus the mean of the visible bins, `rtl_scope_peak_snr_db`)
     and a threshold measured from a full 40-window sweep of 76.5–107.7 MHz:
     the empty Japanese sub-band read **6.2–9.8 dB**, occupied US channels
     **16.3–33.2 dB**. `kFmSeekMinSnrDb = 15.0f`.
  3. *The start frequency clamped instead of wrapping*, and the first window
     is an AGC transient. Seeking up from 107.9 started at the clamped band
     edge and "found" 107.7 — behind where it started. `first` now wraps, and
     window 0 is discarded (windows are 2.048 MHz wide and step 800 kHz, so
     window 1 still covers from origin+176 kHz — no reachable station is
     lost).

  Verified across the band in both directions: 88.1↑89.3, 94.1↑95.5,
  101.3↑102.3, 107.9↑89.3 (wraps past the empty sub-band), 98.5↓96.3,
  88.1↓106.9. All are valid US channels.
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

### 5.7a GMRS/FRS band and the channel scanner

Added a 30-channel GMRS/FRS band (1–22 plus the R15–R22 repeater inputs) and
gave all three channelised bands — CB, GMRS, weather — a SCAN button that walks
the channel list, stops on a busy channel, and resumes ~2.5 s after it goes
quiet. Receive-only; the channel names are identification help, not authority
to transmit.

**Deciding "busy" is the whole problem, and the obvious test is wrong.** The
tuner hears 960 kHz at once, which on GMRS is 38 channels of spectrum. Measured
with one NOAA transmitter on 162.550: *all seven* weather channels read 54–58 dB
SNR, so a level- or SNR-only detector stops on every one of them. A channel is
busy only when the strongest visible bin sits within half a channel of the dial
(that is what separated 162.550 from its six neighbours, whose peaks were
26–150 kHz off centre), the reading clears the squelch, and it stands
`min_snr_db` above the mean of the visible bins.

Thresholds are measured, not guessed, and the measurements are worth keeping:

| Band | Noise/artefact ceiling | Floor set | Why |
|---|---|---|---|
| GMRS 462/467 MHz | 11.6 dB over 153 samples | 20 dB | Largest of 256 noise bins, by chance alone |
| CB 27 MHz | 17.0 dB over 25 samples | 25 dB | LO-leakage skirts at the bottom of the R820T's range |
| Live NOAA signal | 60 dB | — | Scale reference: real signals have room to spare |

Four traps found in the process, all of which would bite anything else built on
this scan engine:

- **`Engine::service()` calls `measure()` and then `finish(completed)` in the
  same call on the last index**, so a hit on a pass's final channel arrived as
  "completed" and was discarded — and since a sweep starts just after the dial,
  that is the channel the user was already listening to.
- **The tuner's LO leakage sits permanently in the centre bin**, looking like a
  carrier exactly on the dial on every channel; it stopped a CB scan on channels
  18–22 alike. The DC bin is now interpolated from its neighbours. Note the
  leakage is *wider* than one bin — after notching, CB still reads 13–17 dB with
  the peak at ±1 bin, which is why CB's floor is raised rather than the notch
  widened; widening it would remove the bins a real on-channel signal lives in.
- **`rtl_scope_peak_level` is not dBFS.** `draw_spectrum` leaves it as raw
  `10*log10` of bin power, so it reads about +55 where the squelch default is
  −75. Comparing the two is a test that can never fail; the FM seek still does
  this. Only its ratio to the mean (the SNR) is meaningful. Use `rtl_signal_dbfs`
  where a real dBFS level is wanted.
- **The spectrum refreshes 9.4–9.8 times a second**, so two windows take ~210 ms.
  A 250 ms dwell left 40 ms of margin and really did read one channel late — it
  stopped a NOAA sweep on 162.400 for a signal on 162.550. Dwell is 400 ms now,
  and readings are freshness-checked against `rtl_scope_measurement_seq` rather
  than trusted to arrive in time; `stale_skips` counts any that are not.

`RTL_CHANNEL_PROBE` prints the detector's own reading for the channel on the
dial — level, SNR, threshold, peak offset, tolerance — which is how all of the
above was measured and how a user calibrates against their own noise floor.

`Id::gmrs` is appended to the dashboard enum rather than inserted next to
`Id::cb`, because those values are persisted to NVS; grid position comes from
`kEntries` order instead.

### 5.7b Marine VHF, channel lockout, and SAME alert decoding

Three additions built on the channel-scan work.

**Marine VHF** was a browse-band preset parked at 156.800 with no plan behind
it. It is now a real channelized band carrying the 40-channel US monitoring
set, reusing ChannelPlan, the scanner, the detector, NFM at 25 kHz and the
squelched audio path. Its frequencies are two interleaved arithmetic series
(channels 1-28 at 156.050 MHz + (n-1) x 50 kHz, 60-88 at 156.025 MHz +
(n-60) x 50 kHz) whose closest pair is 25 kHz apart, which is the spacing the
plan reports. `channel_plan_self_check()` re-derives every entry from those
formulas rather than trusting a 40-entry table, and caught a 25 kHz step in the
first version of the check itself.

**Channel lockout** closes the hole that made the scanner unusable unattended:
one permanently busy channel parked the sweep forever. Locked channels are
dropped when the pass is built, so the engine never dwells on one; locking
everything is refused rather than starting an empty sweep; and locking the
channel a hold is parked on releases that hold instead of stranding the
receiver there. Addressed by the name the dashboard prints ("19", "R15",
"22A", "WX3"), which is why names moved into ChannelPlan and CB and weather
gained name tables.

**SAME/EAS decoding** (`ui/same_decoder.{hpp,cpp}`) runs automatically on the
weather band, fed the same demodulated audio the speaker gets. 520.833 bit/s
AFSK, mark 2083.33 Hz and space 1562.5 Hz, 8-bit ASCII LSB-first with no
framing. Audio is decimated 48 kHz to 8 kHz, quadrature-correlated against both
tones, and sliced at the middle of each bit.

Three things worth inheriting from building it:

- **Sample mid-bit, not on the boundary.** The clock is pulled so transitions
  land on the bit boundary, which makes the boundary the worst place to decide.
  The first version sampled there and decoded nothing at all.
- **A burst ends only at the first non-printable byte,** which needs a full byte
  time of carrier drop. The self-check fed 400 samples of trailing silence --
  4.3 bit times -- and produced nothing; `Decoder::flush()` now exists for a
  caller that knows the stream ended, and the check feeds 1200.
- **There is no host build for this firmware,** so the algorithm was validated
  first as a Python model with identical constants
  (`scratchpad/same_model.py` pattern), then ported. Both bugs above were found
  in the model or by `self_check_detail()`, which names the failing stage --
  worth reaching for before another flash-and-guess cycle.

**Measured on air, 2026-09-09.** Marine inherited GMRS's 20 dB stop threshold
without anyone measuring 156 MHz, so it was probed: 4 reads on each of the 40
channels, 160 in total. Noise ceiling 9.8 dB, nothing above it, zero reads
busy. Marine is the quietest of the three bands -- no LO-leakage problem like
CB has at 27 MHz -- so 20 dB stands with 10.2 dB of margin, more than GMRS has.

| Band | Noise/artefact ceiling | Floor | Margin |
| --- | --- | --- | --- |
| Marine 156 MHz | 9.8 dB (160 reads) | 20 dB | +10.2 |
| GMRS 462 MHz | 11.6 dB (153 samples) | 20 dB | +8.4 |
| CB 27 MHz | 17.0 dB (25 reads) | 25 dB | +8.0 |

**The SAME decoder does not false-alarm on speech.** Six minutes on the live
local NWR transmitter (162.550 MHz, synthesised NOAA voice, which is broadband
audio sitting right where the AFSK tones live) produced zero preamble locks,
zero headers and zero rejected bursts. It never even locked, which is a
stronger result than locking and then rejecting. A spurious tornado warning is
far worse than a missed one, so this is the failure mode that mattered most.
A true positive still needs a real alert or a Wednesday weekly test.

The self-check synthesises bursts and streams them through a real decoder in
256-sample blocks. It buffered the whole 48,000-sample burst at first and
overflowed internal DRAM at link time; internal RAM really is the constraint
this project keeps running into.

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
- **Patch files must stay LF on Windows.** `build-tab5-idf.ps1` applies fixes to
  `managed_components/` from `tools/patches/*.patch` at build time. Git for
  Windows defaults to `core.autocrlf=true`, which rewrites those files to CRLF
  on checkout; the empty context line in a hunk becomes a lone CR that
  `git apply` cannot classify, and the build dies with `corrupt patch at
  ...:29` before compiling anything — so upstream's ESP-Hosted battery-boot fix
  silently was not in any Windows build. This fork pins `*.patch`/`*.diff` to
  LF in `.gitattributes`; **upstream does not carry that entry**, so a plain
  `hardcoreerik/OrcSDR` clone still hits it on Windows. Deliberately kept local
  rather than sent upstream as a PR.
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

## 7a. Local data: what the published packs cannot give you

Two features depend on data the signed catalog does not carry, and both now
have a builder that fills the gap from a public-domain source. Both write a
`local_*.idx` the firmware loads in preference to the packaged file, so the
signed packs are never mutated.

| Feature | Gap in the published pack | Builder |
| --- | --- | --- |
| Offline map | only `lane_county_map` exists | `tools/build_orcmap.py` (OpenStreetMap / Overpass) |
| Listen to ATC | `faa_aviation` has 40,937 frequency records but **no coordinates**, and none of the optional `ATC` preset lines | `tools/build_atc_presets.py` (OurAirports) |

`docs/LOCAL_SETUP.md` has the commands. Both were verified end to end on
hardware for Woodbridge, VA: the LoRa map view reads "OFFLINE LOCAL MAP" and
the ADS-B card reads "KDAA TWR 126.300 / TAP TO LISTEN / READY".

## 7b. Meshtastic frequency slots

The US band is **104 slots of 250 kHz from 902.125 MHz**, so the firmware's
old fixed 906.875 MHz is slot 20 (LongFast default). The LoRa dashboard's
CHANNELS button opens a picker (step, quick slots, LONGFAST marker); the same
is available over serial as `RTL_UI ACTION LORA SLOT <1-104> | SLOT_PREV |
SLOT_NEXT`. Before this the monitor could only ever watch one slot.

Note for anyone touching that overlay: the LoRa spectrum and waterfall repaint
on their own timer and will draw straight through a panel unless
`spectrum_active()` is false while it is up.

## 8. Still open

1. **SDIO transport wedge** — root cause found (§3a): USB DMA from a streaming
   RTL-SDR contending with the Wi-Fi transport's PSRAM DMA. Worked around by
   pausing reception around Wi-Fi work, verified 10/10. Still worth knowing
   that a wedged link needs a restart, and that a **full power cycle** clears
   states a P4 reset does not.
2. **Reception cannot run during a download.** Removing that limitation means
   `CONFIG_EH_HOST_PORT_DMA_PREFER_SPIRAM=n` and a careful internal-RAM
   budget — see §3a.
3. **`noaa_weather` / `fcc_broadcast` packs** cannot be published from this
   fork — the catalog is signed with the upstream author's P-256 key and the
   firmware embeds only the matching public key.
4. **Resolved: every SD transfer operation requires `PAIR`/`AUTH`.** Listing,
   reading, writing, and deleting are all inside the authenticated boundary
   because `/orcsdr/` can contain recordings, location data, network settings,
   and decoder logs. All supported transfer tools perform the HMAC handshake
   from the untracked pairing-key file.
5. **No channelized band has met a real transmission yet.** The detector is
   proved to reject noise on all four -- CB, GMRS, weather and marine were each
   measured against their own noise floor -- and proved to accept a real signal
   on weather only, where a live NOAA transmitter held and resumed correctly.
   The SAME decoder is separately proved not to false-alarm on live weather
   voice (six minutes, zero locks) but has never seen a real alert header.
   What is untested everywhere else is the *accept* half: whether the floor
   lets a genuine handheld a few streets away through. If a scan walks past
   traffic you can hear, that number is the one to lower -- read it with
   `RTL_CHANNEL_PROBE` while the signal is up, and see 5.7a/5.7b.

   The original GMRS wording follows, still accurate:

   5. **The channel scanner's GMRS threshold has not met a real GMRS signal.**
   Both halves were verified, but on different bands: "stops on a real signal"
   was proved on weather (a live NOAA transmitter, 60 dB, held and resumed
   correctly), and "does not stop on noise" was proved on GMRS and CB (93 and
   104 samples, zero false stops). Nothing was transmitting on GMRS or CB while
   this was built, so the 20 dB GMRS floor is known to reject noise but has
   never been confirmed to *accept* a handheld a few streets away. If a scan
   walks past traffic you can hear, that number is the one to lower — check it
   with `RTL_CHANNEL_PROBE` while the signal is up, and see §5.7a for what the
   three tests mean.
6. **Resolved: FM preset scan and seek now use in-window SNR.** The old
   `rtl_scope_peak_level`/dBFS comparison could never fail because the spectrum
   value is raw bin power. Both paths now gate on `rtl_scope_peak_snr_db` and
   retain raw level only for ranking and diagnostics — see §5.7a.
7. **The battery boot loop is masked, not fixed** — §3b. A timing-sensitive
   heap corruption in the hosted SDIO path, held off by
   `CONFIG_FREERTOS_WATCHPOINT_END_OF_STACK=y`. Changing FreeRTOS or heap
   config, or anything about Wi-Fi bringup timing, can bring it back; the
   `heap_guard()` checkpoints exist to name the stage when it does.
8. **Resolved in the fork: the Hosted task workaround no longer leaks.**
   Detached one-shot tasks use ordinary `xTaskCreate` and let the idle task
   reap them. Joinable WithCaps tasks signal completion, suspend, and are
   deleted from the owner task by `vTaskDeleteWithCaps`, which frees their
   static TCB and SPIRAM stack without using IDF 5.5.4's unsafe self-delete
   path. The battery race in §3b remains a separate, masked issue.

Audited clean: buffer handling (every `memcpy`/`strcat` bounds-checked, no
`strcpy`/`sprintf`/`gets`), path traversal (`..` rejected on both the SD and
catalog paths), the catalog chain (signature verified *before* the manifest is
parsed, SHA-256 before activation, `https://` enforced, destinations pinned
under `/orcsdr/data/`), the `AUTH` proof comparison (constant-time XOR
accumulate, no early exit), and no secrets on the serial console.

---

## 9. Map of the interesting files

| File | What lives there |
| --- | --- |
| `ui/main.cpp` | Radio session, streaming task, serial CLI, generic radio screen, scan drivers, snapshot builders. Everything not in a dashboard. |
| `ui/orc_console.*` | Buffered USB Serial/JTAG transport extracted from `main.cpp`; serial command routing remains in the monolith. |
| `ui/ui_theme.hpp` | Shared RGB565, radius, and minimum-touch-size tokens for product chrome. |
| `ui/waterfall_view.{hpp,cpp}` | Ring-buffer waterfall. Added by this fork. |
| `ui/wifi_service.{hpp,cpp}` | Station lifecycle, `reset_link()`, transport health, disconnect reasons. |
| `ui/catalog_sync.{hpp,cpp}` | Signed data catalog: fetch, verify, install, remove. |
| `ui/home_dashboard.cpp` | Home *and* every band without a dedicated dashboard (CB, weather, shortwave, browse). |
| `ui/p25_config.{hpp,cpp}` | P25 profile parse/validate/store. Strict parser — read it before writing a profile. |
| `ui/scan_engine.{hpp,cpp}` | Generic non-blocking retune/measure sweep with a host self-check. |
| `ui/offline_map.{hpp,cpp}` | ORCMAP1 loader. User map wins over the packaged one. |
| `tools/build_orcmap.py` | Build a map for your area. Added by this fork. |
| `docs/LOCAL_SETUP.md` | User-facing: data packs, maps, P25, location. Added by this fork. |

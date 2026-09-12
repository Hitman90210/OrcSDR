# Feature status

What is verified on real hardware, what is prototype, and what is not there at
all. The README points here before you rely on a decoder, so it tries to be
blunt rather than flattering.

**Implemented** — used on live signals and behaves.
**Experimental** — works on the bench, thinly tested, expect rough edges.
**WIP** — partly there; the gaps are named.
**Deferred / Unavailable** — not built, or not possible on this hardware.

## Receive and decode

| Feature | Status | Notes |
|---|---|---|
| FM receive, stereo audio, presets, health | Implemented | Live hardware verified |
| FM RDS decoding | Implemented | Sequential A–B–C–D groups, confirmed PS, voted PTY, Radio Text. Needs a 260 kHz FM filter. This dongle uses a +13 kHz LO bias that is not shown as the channel. |
| AM broadcast dashboard | Implemented | From upstream: channel-aware tuning, bounded gain, presets, and a wideband station finder. Restores its own saved frequency on band change. **Bench-verified 2026-09-12**: 12/12 samples across 3 frequencies × 4 filters, entry/gain/AM-to-FM-transition regressions all pass, and the 2.4 MS/s wideband finder swept 119 channels and returned 6 real stations — 0 overruns, 0 drops throughout. |
| ADS-B 1090 dashboard and aircraft database | Implemented | Live hardware verified; coverage depends on antenna, location, and valid position messages |
| P25 control and clear voice following | WIP | Phase I clear voice verified; wider system compatibility in progress. Encrypted voice is not decoded. Phase II audio is not implemented. |
| POCSAG paging | Experimental | Decoder and store verified against synthetic vectors; live traffic depends on a local transmitter |
| Weather (NOAA WX) | Implemented | Seven channels; all seven measured 54–58 dB SNR on the bench |
| SAME / EAS alert decoding | Experimental | 520.833 bit/s AFSK decoder with a self-test. Verified against synthetic headers; **not yet caught a live alert** — the weekly test is the thing to try. |
| CB (40 channel) | Implemented | Channel names, scanner, per-channel lockout |
| GMRS / FRS (30 channel) | Experimental | 462/467 MHz, correct 12.5 kHz interstitial spacing. Scanner works; not yet checked against a real handheld. |
| Marine VHF (40 channel) | Experimental | Channel plan verified against the 50 kHz channel-number step; Ch 16 not yet listened to on air |
| Channel scanner with lockout | Implemented | Stops on a busy channel using in-channel peak offset, not raw level. 400 ms dwell with a freshness counter. Lockouts persist per band. |
| Shortwave / Browse | Experimental | Shared radio/scope/capture foundation |
| Aeronautical AM (108–137 MHz) | Implemented | The AIRBAND tile tunes Browse to 121.5 MHz. Browse now demodulates **AM across 108–137 MHz** — 118–137 is voice, 108–118 is VOR/ILS, and all of it is double-sideband AM (ICAO Annex 10). Before this it ran an FM discriminator over the whole band, which on an AM carrier produces noise. The filter narrows to 10 kHz there, 25 kHz elsewhere. Reception against a live aircraft or tower is still untested. |
| Modulation labels | Corrected | The mode readout said NFM for every non-broadcast FM band, which is scanner shorthand, not what is on the air. Now: **FM** (±5 kHz) for NOAA weather (16K0F3E), marine VHF (16K0G3E), amateur 6 m/2 m/1.25 m/70 cm, and GMRS except channels 8–14; **NFM** only where the signal really is narrowband — GMRS 8–14, the 467 MHz interstitials that [47 CFR §95.1773](https://www.ecfr.gov/current/title-47/chapter-I/subchapter-D/part-95/subpart-E/section-95.1773) limits to 12.5 kHz while everything else in GMRS gets 20 kHz — plus narrowbanded land mobile under Browse. **Bench-verified 2026-09-12**: 14 frequencies across every band, all correct. |
| Wi-Fi Analysis (2.4 GHz) | Experimental | Survey only |

## LoRa / Meshtastic

| Feature | Status | Notes |
|---|---|---|
| LoRa receive, packet views, capture | Experimental | Receive-only, always |
| LongFast (250 kHz, SF 11) | Implemented | A legacy-interleaved NODEINFO, `TEST` message, node and encrypted-frame counting observed on the bench |
| **Long Turbo / Short Turbo (500 kHz)** | Implemented | Added in this fork. A real Long Turbo transmission decoded end to end with a valid PHY CRC (`packets=1 crc_ok=1 encrypted=1`). |
| Meshtastic preset selector | Implemented | Long Fast / Long Turbo / Short Turbo, persisted, applying SF, bandwidth, regional slot count and the official channel-name hash together |
| Meshtastic 2.8 long interleaving | **Not decoded** | Detected and counted as `li_headers` rather than reported as an RF failure. LR11x0 and SX128x radios on 2.8 use it; those payloads need a long-interleaver this fork does not have. Nodes on 2.7.x or legacy interleaving decode normally. **Check `li_headers` before blaming this**: across 30+ on-air captures on one bench it stayed at `0`, so nothing nearby was using it and every failure there had another cause. Read `snr_db` on the same line — see `API_SERIAL_CLI.md`. |
| DSP regression vector | Implemented | 10 synthesised-symbol cases across 125/250/500 kHz and both front-end directions; `RTL_LORA_SELFTEST` |

## Interface and platform

| Feature | Status | Notes |
|---|---|---|
| Global on-device Settings | Implemented | Wi-Fi and Companion remain optional |
| Companion LAN console | Implemented | Mode-aware: ADS-B radar with up to 16 nearest targets, location-gated relative plotting, explicit validity for zero-valued measurements, and climb/descent arrows; a channel faceplate for CB/GMRS/marine/weather; spectrum with RDS station name and Radio Text elsewhere. **View-and-listen by default**; visitor control is a separate opt-in switch and is refused at the server (403) when off. No TLS and no login either way, so trusted networks only. |
| Android TV viewer | Experimental | Sideload `apps/orcsdr-tv` on Android 9 TV. Unplug the PC flash/JTAG USB cable after flashing — that cable, not general power, is the bench brownout trigger. |
| Signed data packs | Implemented | See [maps and data](maps-and-data.md). **Bench-verified 2026-09-12**: catalog fetched over Wi-Fi, P-256 signature verified, manifest parsed, all five packs enumerated with correct installed state. Offline, a catalog check is now refused with "Connect Wi-Fi before downloading" instead of starting a download that cannot succeed. |
| Offline maps | Implemented (build your own) | The catalog's only map is upstream's home county. `apps/orcsdr-tab5/tools/build_orcmap.py` builds one for your area from OpenStreetMap; the device prefers it. See [maps and data](maps-and-data.md). |
| Battery operation | Implemented, with a caveat | A boot loop on battery was traced to heap corruption in the hosted SDIO path and is **masked, not fixed** (`CONFIG_FREERTOS_WATCHPOINT_END_OF_STACK`, plus a longer C6 boot delay). See `FORK_HANDOFF.md` §3b. |
| Bluetooth speaker audio | Unavailable | The Tab5's C6 supports BLE, not Classic Bluetooth A2DP output |
| Companion phone integration | Deferred | On-device operation never depends on it |

## Known gaps worth naming

- **Meshtastic 2.8 long interleaving.** The largest functional gap. Your own
  node may decode while a neighbour's does not, purely by firmware version.
- **SAME/EAS has never decoded a live alert.** The decoder and its self-test
  pass; the on-air half is unproven.
- **GMRS and marine have not met a real transmitter.** Channel plans are
  verified by arithmetic, not by listening.
- **The battery boot loop is masked.** It is not understood.
- **P25 Phase II audio and USB hub support** are not implemented.

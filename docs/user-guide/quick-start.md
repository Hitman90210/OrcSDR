# Quick start

**Goal: hear a radio station within five minutes of opening the box.**

Everything optional — Wi-Fi, SD card, databases, maps — is left to the
[full guide](index.md). None of it is needed to receive.

---

## What you need

| | |
| --- | --- |
| **M5Stack Tab5** | the radio |
| **RTL-SDR Blog V4** | the receiver, into the Tab5's **USB-A** port |
| **An antenna** | whatever suits the band; the stock whip is fine for FM |
| A microSD card | *optional* — only for databases, maps and recordings |

> The **USB-C** port is for power and flashing. The **USB-A** port is where the
> dongle goes. They are not interchangeable.

## 1. Flash it

Open **M5Burner**, search **OrcSDR**, flash to the Tab5 *without erase*. That is
the whole install. (Building from source instead?
[Getting started](getting-started.md) has that path.)

## 2. Plug in and power on

Dongle into USB-A, antenna on the dongle, press power.

Let the splash finish — it stages the USB host and the receiver, which takes a
few seconds. Then tap **OrcSDR**.

**If you flashed over USB-C from a PC, unplug that cable now.** Leaving it
attached is the single most common cause of brownouts under Wi-Fi plus receiver
load, and it looks like random instability rather than a power problem.

## 3. Hear something

You land on **Home**. Tap **FM RADIO**.

It comes up on **96.1 MHz**. To find a station you can actually hear:

- Drag the **frequency dial**, or
- Tap **NAV** and type a local station's frequency directly.

You should see the **spectrum** move, the **SIG** bar rise on a real station,
and hear audio. If a station is strong you will also see **STEREO** and, after a
few seconds, the station's name under **RDS**.

**No sound?** Tap the speaker icon in the header — volume and mute are separate,
so turning the level up does not unmute. Check the antenna is on the dongle and
not the Tab5.

## 4. Try the things that make it interesting

| Tap | What you get | Needs |
| --- | --- | --- |
| **ADS-B** | Aircraft overhead on a radar, with a list and per-target detail | A 1090 MHz antenna helps a lot |
| **WEATHER** | The seven NOAA channels, and SAME alert decoding | Nothing |
| **CB** / **GMRS** | Channel-by-channel with a **SCAN** button that stops on activity | Nothing |
| **LORA** | Passive Meshtastic monitoring | Nothing |
| **RF LAB** | Measurements and a live test bench | Nothing |

On the channelised bands, **SCAN** steps the list, parks on a busy channel, and
moves on about 2.5 seconds after it goes quiet. Locking a channel out makes the
sweep step over it.

## 5. When you want more

Each of these is optional and independent:

- **Aircraft names, airports, station IDs** — connect Wi-Fi, then
  **Settings → Data & Maps → Check for Updates**.
  See [maps and data packs](maps-and-data.md).
- **Roads and water under the ADS-B radar** — build a map for your own area.
  Same page; there is no map pack to download for where you live.
- **Watch and listen from a browser or TV** — **Settings → Companion → Web
  Console → ON**, then open the URL it shows. It is view-and-listen only unless
  you also switch **Visitor Control** on.
- **Your local Meshtastic settings** — the LoRa dashboard's **CHANNELS** button
  picks the preset and slot, and remembers them.

## If something is wrong

| Symptom | First thing to check |
| --- | --- |
| No spectrum, "NO USB" | Dongle in **USB-A**, not USB-C. Re-seat it. |
| Random reboots or freezes | Unplug the PC USB-C cable. See step 2. |
| Audio silent | Speaker icon: mute and volume are separate controls |
| A decoder finds nothing | Check [feature status](feature-status.md) — some decoders are prototype, and it says which |
| Something else | [Troubleshooting](troubleshooting.md) |

## Before you rely on it

This fork is an active development project. **[Feature
status](feature-status.md)** lists what is verified on real hardware, what is
prototype, and what is not implemented — including a plain list of the known
gaps. Worth two minutes before you trust a decode.

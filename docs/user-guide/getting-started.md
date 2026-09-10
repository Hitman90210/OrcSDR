# Getting started

> **Just want to use the radio?** [Quick start](quick-start.md) gets you from
> box to audio in five minutes. This page covers the hardware detail and the
> build-from-source path.

## Hardware

1. Seat the Tab5 securely and connect a supported RTL-SDR Blog V4 to the USB-A host port.
2. Attach the antenna appropriate for the band you intend to receive.
3. Insert the prepared microSD card for databases, maps, captures, and screenshots.
4. Use a stable power source. A depleted external battery can remain an electrical load even when USB is attached. After flashing, unplug the PC USB Serial/JTAG cable (the COM port used by `install-orcsdr.ps1`). Leaving that cable connected can brownout the Tab5 under Wi-Fi + RTL load even when battery or wall power is otherwise fine.
5. Press the Tab5 power button and allow the staged USB-host and receiver startup to finish.

## First boot

Boot lands on Home. If Auto-start reception is on, the last FM station can run in the background. Wi-Fi and phone pairing are optional; reception does not require either.

## Installation

Current development uses ESP-IDF 5.5.4 and ESP-Hosted 3.0.6. The native P4
application/radio path and the matching P4-to-C6 Wi-Fi handshake have been
verified on Tab5 hardware. Follow the exact build, flash, and status guidance
in [`Tab5 ESP-Hosted 3.0.6 migration`](tab5-esp-hosted-3-migration.md).

Do not use the legacy 2.12.6 installer flow as a 3.0.6 verification step.
For a native build:

```powershell
Set-Location .\apps\orcsdr-tab5
.\tools\build-tab5-idf.ps1 -IdfPath 'C:\Espressif\v5.5.4\esp-idf'
```

The Tab5 C6 must run matching ESP-Hosted **3.0.6**. A normal P4 application
flash does not silently update the C6. Do not claim a new device accepted until serial prints
the three `I OrcSDR` C6/version/transport lines in the migration document.

Do not use PlatformIO for Tab5 firmware. Preserve a recovery image before replacing known-good firmware.

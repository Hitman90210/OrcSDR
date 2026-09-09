# Issue #66 diagnostic notes (grok/fix-66-heap-poison)

Branch commit cfcd72b already enables CONFIG_HEAP_POISONING_COMPREHENSIVE +
CONFIG_HEAP_TASK_TRACKING. This note records code-backed findings from the
2026-09-08 analysis pass. Analysis only; not flashed.

## managed_components

**Absent** in this worktree (and `build-native-hosted3/` is absent).
`dependencies.lock` pins `espressif/esp_hosted` **3.0.6** and
`espressif/esp_wifi_remote` **1.6.4**. Upstream esp-hosted-mcu SDIO RX
symbols were cross-checked from public **v3.0.7** sources (no v3.0.6 git
tag on GitHub); structure matches the handoff names.

## Causal chain (post-PR #71)

1. `setup()` → `M5.begin` → (if `set_wifi_power`) `initialize_wifi()` →
   `prepare_wifi_coprocessor()` (IO expander WLAN_PWR_EN 100 ms off / 200 ms on)
   → `orcsdr::wifi::start()` → `esp_hosted_init()` + `esp_hosted_connect_to_slave()`.
2. Hosted SDIO bring-up starts `sdio_process_rx_task` (streaming mode).
3. Heap free-list metadata is corrupted in the SDIO RX / dispatch path
   (handoff coredump: `sdio_process_rx` → TLSF `block_locate_free` during
   `initialize_wifi` / `eh_host_connect_to_slave`).
4. Assert/panic → `ESP_RST_PANIC` → reboot → repeat (light-blue flash loop).
5. PR #71 (`vTaskDeleteWithCaps` trampoline → `vTaskDelete(NULL)`) removed the
   earlier `eh_auto_init` abort; this TLSF path remains.

## Why PC USB-C changes the outcome (code-supported, not speculation-as-fact)

1. **USJ console drain / block (timing modulator)** —
   `CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG=y` and `OrcConsole::write` calls
   `usb_serial_jtag_write_bytes(..., 100 ms)` (`main.cpp` ~196). With a PC
   host draining TX, early `Serial`/`ESP_LOG` during Hosted connect complete
   quickly; without a host the same path waits/yields differently. That was
   ruled out as the *PMIC hold* sole cause, but it still changes scheduling
   relative to SDIO RX tasks. Pointers: `main.cpp:124-196`, `13842-13873`,
   `sdkconfig.defaults` `CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG`.

2. **USB ISR / core load** — USB ownership is pinned away from Hosted/UI
   (`main.cpp` ~7870 comment: usb=core0, dsp/ui/hosted=core1). Attached PC
   USB generates USJ IRQs on core0; battery/charger does not. Different IRQ
   and cache pressure changes interleaving of `sdio_read` /
   `sdio_process_rx_task` vs `app_main` heap use.

3. **Tiny Hosted RX queue under streaming** —
   `CONFIG_ESP_HOSTED_HOST_SDIO_RX_Q_SIZE=2` (upstream default 20) with
   `CONFIG_ESP_HOSTED_HOST_SDIO_RX_STREAMING_MODE=y`. Upstream Kconfig notes
   staging depth ≈ slots−1 (default 2 → depth 1). Any timing shift that
   bursts RX during `connect_to_slave` exercises drop/free/realloc paths
   harder when RX_Q=2.

4. **Supply / VBUS (secondary)** — sdkconfig comments document PC USB-JTAG
   VBUS sag vs BOD; BOD is off. Charger-also-fails rules out “need PC to
   latch PMIC”, but does not rule out rail/noise differences affecting SDIO
   bit integrity (known Hosted failure mode: bad PKT_LEN → huge/corrupt RX
   alloc). Keep as secondary, not primary.

Not the #210 wifi_remote mismatch: lock has `esp_wifi_remote` 1.6.4 (≥1.3.1).

## Recommended single next experiment

**Flash this branch’s heap-poison build and capture one battery/charger
coredump** (`idf.py -B build-native-hosted3 coredump-info`). Expect
poisoning to abort at the *writer* that clobbers free-list metadata instead
of later at `block_locate_free`/`malloc`. Do not change RX_Q or skip-wifi
until that dump names the site — poison is already staged in
`sdkconfig.defaults`.

Fallback A/B if poison dump is inconclusive: bump only
`CONFIG_ESP_HOSTED_HOST_SDIO_RX_Q_SIZE` 2→20 on this branch and retest
battery boot (isolates queue-depth / backpressure).

## Code pointers

| Item | Location |
|------|----------|
| setup / reset reason / M5.begin | `apps/orcsdr-tab5/ui/main.cpp` ~13842–13873 |
| Hosted-before-SD order + `initialize_wifi()` | `main.cpp` ~14040–14059, ~7943–7976 |
| C6 power cycle | `main.cpp` `prepare_wifi_coprocessor` ~7930 |
| OrcConsole USJ 100 ms write | `main.cpp` ~124–196 |
| `esp_hosted_init` / `connect_to_slave` | `apps/orcsdr-tab5/ui/wifi_service.cpp` `start()` |
| RX_Q=2, streaming, USJ console, poison | `apps/orcsdr-tab5/sdkconfig.defaults` |
| PR #71 trampoline patch | `apps/orcsdr-tab5/tools/patches/esp-hosted-trampoline-null-delete.patch` |
| Upstream SDIO RX (v3.0.7 stand-in) | `host/mcu/eh_host_mcu_transport/src/eh_host_bus_sdio.c`: `serial_rx_handler`, `sdio_process_rx_task`, `sdio_streaming_push_data_to_queue` |


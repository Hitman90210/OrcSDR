# Map cache writeback A/B gate

Status: trial completed; radio-continuity gate failed. Option removed from
shipping defaults. Preserve measurements for further diagnosis.

## Evidence and hypothesis

COM17 app 5e79fd44, Blog V4, indoor 915 MHz whip, US LongFast slot 20 at
960 ksps: idle driver drops stayed 39; opening the picker raised them to 54;
successive pans raised them to 64, 76, 76. Raw phase log:
`cache-writeback-A-device-20261025.log`, beside the script invocation.

M5GFX Panel_FrameBufferBase::display coalesces the dirty framebuffer region and
calls Cache_WriteBack_Addr -> esp_cache_msync(C2M). ESP-IDF 5.5.4's default
unchunked s_c2m_ops holds a critical section for the whole range. Its existing
chunked option releases the critical section between chunks. Radio delivery and
UI both run on core 1. This is a causal hypothesis, not an established fix.

## Scope and safety

Enable only CONFIG_ESP_MM_CACHE_MSYNC_C2M_CHUNKED_OPS and its P4 default 128 KiB
chunk size. Preserve the 40 KiB DMA reserve, SDK version, receiver/DSP source,
driver, map engine, C6 input and runtime settings. Do not modify managed files.

The SDK warns about an ISR accessing the same buffer during writeback. OrcSDR
draws through the UI owner; no framebuffer-writing ISR was found in the inspected
P4 display paths. DMA scanout is already asynchronous. Physical appearance,
radio continuity, SD readback and Wi-Fi operation remain acceptance gates; a
configuration probe alone proves none of these.

## Steps

1. Kconfig-only probe: require exactly the two intended macros to differ from
   the saved native header, using the same SDK environment and rename inputs.
   This passes; logs `cache-writeback-kconfig-probe-final-20261025.log` and
   `.local/maps/cache-writeback-probe-final-20261025/result.json`.
2. Native rebuild and inspect effective flags, unchanged reserve, placement,
   dependencies, public-key/catalog and existing symbol gates.
3. COM17 app-only flash with previous-app verification, fresh non-app backups,
   post-flash preservation checks and rollback. No erase/C6 change; COM39 stays
   untouched for this trial.
4. Repeat `.local/maps/cache-writeback-ab-plan-20261025.json` and compare losses
   by phase, sample throughput, map timing, memory and decoding. Repeat at FM's
   2.4 MSPS for continuity only; the 915 MHz antenna is not FM decode acceptance.
5. Check retained-map SD write/readback and map appearance. Reject or investigate
   errors, corruption, worse rendering or continued losses; do not convert them
   into a release pass. Preserve accepted drag performance as its separate gate.

Earlier bounded Grok review of the memory diff and this hypothesis returned
CLEAN; that is not hardware proof. Existing PR review/storage findings and the
full decoder/dongle/package matrix remain open. Use PR #211; no new PR or tag.

## Actual outcome

The matched 960 ksps sequence measured A2: 55 driver / 1,101 application drops;
B: 14 driver / 472 application drops. Both idle windows had zero new drops.
This is partial mitigation, not resolution. Map completion also differed between
runs, so these numbers do not establish a general speed multiplier. The B image
passed boot, non-app preservation and an 8 KiB SD write/readback while map memory
was retained and radio paused. FM and physical-display gates were not completed.
See `docs/evidence/maps/2026-10-25-cache-writeback-trial.md`.

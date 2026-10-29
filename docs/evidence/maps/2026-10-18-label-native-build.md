# Label integration — native build attempt

October 18, 2026. User approved one full native build and inspection of
`8d1eaa48` plus preserved wizard edits, using OrcMaps `5f3a1eb`.
No flash, erase, C6 update, push or merge was authorized or performed.

## Actual result: build failed before linking

The first configuration attempt lacked the cached RISC-V compiler directory
in PATH. After correcting PATH, the build regenerated configuration without
the previous SDKCONFIG/cache inputs and failed compiling ESP-Hosted Wi-Fi
initialization due to missing CONFIG_WIFI_RMT_* symbols. Explicitly selecting
the existing Hosted-3 sdkconfig and C6 image did not resolve those missing
symbols. No new firmware size or PSRAM-placement result can be claimed.

Logs are saved in the directory from which commands ran:
`apps/orcsdr-tab5/label-native-config-failed-20261018.log`,
`apps/orcsdr-tab5/label-native-build-20261018.log`, and
`apps/orcsdr-tab5/label-native-build-configured-20261018.log`.
The first log was moved there after initially being redirected outside that
directory. No logs or recovery artifacts were deleted.

## Narrow diagnosis

ESP-WiFi-Remote's Kconfig conditionally loads
`Kconfig.idf_v$ESP_IDF_VERSION.in`. The build environment lacked
ESP_IDF_VERSION; generated config.env supplied IDF_VERSION=5.5.4, a different
variable. The component's version-specific buffer options were omitted.

Copied sdkconfig to a local probe file, then ran only configuration generation:

```
C:/Espressif/python_env/idf5.5_py3.14_env/Scripts/python.exe -m kconfgen --kconfig C:/Espressif/frameworks/esp-idf-v5.5.4/Kconfig --config label-kconfig-probe.config --env-file build-native-hosted3/config.env --list-separator semicolon --env ESP_IDF_VERSION=5.5 --output header label-kconfig-probe.h
```

This focused probe passed and generated:

- CONFIG_WIFI_RMT_STATIC_RX_BUFFER_NUM=16
- CONFIG_WIFI_RMT_DYNAMIC_RX_BUFFER_NUM=32
- CONFIG_SPIRAM_MALLOC_RESERVE_INTERNAL=40960

Log: `apps/orcsdr-tab5/label-kconfig-probe-fixed-20261018.log`.
The earlier probe omitted the list separator and failed; its log is preserved.
This establishes the missing-option cause, not a successful firmware build.
No managed component source or Wi-Fi implementation was patched.

## Corrected build — passed after explicit approval

The user approved one corrected build and inspection. The same source candidate
(`330606cb`, a documentation-only successor to `8d1eaa48`, plus preserved wizard
edits) was built with ESP_IDF_VERSION=5.5 and explicit SDKCONFIG, existing C6
image, toolchain PATH and disabled startup diagnostics. Command: the native
ESP-IDF `idf.py -B build-native-hosted3` invocation with those inputs, then
`reconfigure build`. Full command is recorded in this session; complete output:
`apps/orcsdr-tab5/label-native-build-envfixed-20261018.log`.

Actual native results:

- Build/link/image/partition-size checks: **PASS**, 1,626 build steps completed.
- Application: **5,051,280 bytes**; 6 MiB app partition leaves **1,240,176 bytes**
  (20% as reported by ESP-IDF).
- LabelLayout: **15,416 bytes**, address **0x4851e960**, entirely within the
  linked **.ext_ram.bss** section. This verifies placement, not runtime heap health.
- DMA reserve: **40,960 bytes** in generated sdkconfig, unchanged.
- Requested/locked/fetched OrcMaps pin: **PASS**, exact `5f3a1eb767b26aa7df00158308f40331b298d7ef`.
- Miniz gate: **PASS**, two bundled OrcMaps miniz symbols; no OrcMaps use of
  ROM-absolute symbols. Sixteen ROM-absolute symbols exist but are unused by OrcMaps.
- FT8/FT4/JS8 shared PCM scratch: **PASS**, 8,192 bytes in PSRAM at 0x484f7440.
- Application SHA-256:
  `6cdb02ce1a0c89afca2f031c31305edc9547ee2c705c7d017d14e607fca34306`.

`label_native_inspect_20261018.py` ran successfully in apps/orcsdr-tab5; its
log and JSON manifest are beside it. The manifest records the source commit,
tracked dirty-diff hash and hashes of the preserved untracked wizard sources.
BIN, ELF and manifest are preserved under
`.local/maps/labels-330606cb-20261018/` for subsequent approved hardware testing.

Read-only OS port enumeration reports COM17 USB serial 30:ED:A0:E2:E6:95 and
COM39 E8:F6:0A:E2:D6:03. No port was opened and no firmware was queried or changed.

## Next gate — hardware approval and validation

Before a COM17 app-only flash, verify the target and current firmware and preserve
recovery/NVS. Then bound boot, setup/map font, memory, retained-map SD and
active-radio checks. Preserve SD/world/C6 and leave COM39 untouched.
Neither Tab5 has changed. Hardware validation remains pending.

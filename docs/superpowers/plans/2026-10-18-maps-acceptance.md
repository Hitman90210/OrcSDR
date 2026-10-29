# OrcMaps integration acceptance plan

> **For agentic workers:** Use superpowers:executing-plans to execute this ledger inline. Check results before closing each gate.

**Goal:** Qualify the shared OrcMaps integration on ESP32-P4, preserve existing data, and merge only after device checks and both external reviews pass.

**Architecture:** Keep the shared maps adapter and explicit legacy compatibility paths. Fix demonstrated integration failures at their narrowest boundary; retain the existing receiver, USB, DSP and storage architecture.

**Tech Stack:** ESP-IDF 5.5.4, ESP32-P4 Tab5, OrcMaps v0.2.3, C++17, host sanitizer tests.

**Spec:** User's original OrcMaps integration prompt and follow-up requirements; acceptance evidence in `docs/maps/` and `docs/SD_CARD_RELIABILITY.md`.

## Global constraints

- COM39 is a session-authorized P4 flash target, USB serial E8:F6:0A:E2:D6:03. Its C6 must remain on ESP-Hosted 1.4.1 by explicit user instruction. Preserve NVS, SD and recovery artifacts.
- On October 18 the user superseded COM17's comparison-only role and authorized its update after verified recovery inventory. COM17 now runs P4 source 6f001d86; its existing C6 3.0.6 and SD files remain unchanged. See `docs/evidence/maps/2026-10-18-com17-update.md` for the exact old image, backup and open memory gate.
- Preserve legacy ORCMAP1 separately from PMTiles and the current RTL driver pin.
- Raw device logs stay in the test invocation directory; publish reviewed evidence without credentials or private received data.
- Source, host tests, build, flash, device behavior and physical acceptance are separate gates.

## Review focus

- Low internal/DMA memory with retained map buffers must not make SD transfers unsafe.
- Repeated map/FT8 navigation during decode must remain responsive; reset recovery alone is not a fix for the observed hang.
- Failed SD flush/close must not discard staged logs or report a successful save.
- Existing location and data must survive setup cancellation and reboot.
- Wi-Fi and OrcDial checks must use existing credentials/trust and preserve device settings.

## Task 1: Restore FT8 regression coverage

Files: `ui/map_world_compat.hpp`, `ui/map_pipeline.{hpp,cpp}`, `tests/ft8_ui_stubs/`, `tools/test-ft8.sh`.

- [x] Reproduce full FT8 host failure: compatibility header unnecessarily includes PMTiles headers.
- [x] Move unchanged pure projection helper to the compatibility header, link the extracted renderer in the dashboard test, and align graphics stubs with the real namespace.
- [x] Focused dashboard ASan/UBSan, all five map CTests, full FT8 suite and diagnostic suite pass. Logs: `stability-ft8-dashboard-20261018.log`, `stability-maps-host-20261018.log`, `stability-ft8-host-fixed-20261018.log`.

## Task 2: Exercise shared storage and navigation

Files: `docs/SD_CARD_RELIABILITY.md`, reviewed evidence under `docs/evidence/maps/`; change storage code only for reproduced defects.

- [x] Full SSTV synthetic suite and shared SD sanitizer gate passed on 2026-10-18; see root invocation logs.
- [x] Matched read-only status/memory/catalog/FT8/OrcDial captures saved in `stability-com17-comparison-20261018.log` and `stability-com39-comparison-20261018.log`; no reset. COM17 V4 and COM39 V3 differ; both version labels are v0.4.0-beta.1, exact COM17 build remains unverified.
- [ ] Exercise FT8 navigation with map memory retained, actual FT8/SSTV/LoRa saves and readback, memory and radio counters. Investigate failures before claiming qualification.
- [ ] Verify saved new records across an authorized COM39 reboot; do not alter COM17 firmware or reset it.

## Task 3: Close integration and coexistence gates

Files: existing setup/maps adapter and tests; `tools/test_wifi_reconnect_policy.py`, `orcdial/tests/`, `docs/maps/`.

- [x] Wi-Fi policy (1/1) and OrcDial protocol/security/runtime (5/5) host checks pass. Fixed stale OrcDial host include boundary and missing stack-watermark stub. Logs: `stability-wifi-policy-20261018.log`, `stability-orcdial-host-fixed-20261018.log`.
- [ ] Verify current Wi-Fi/OrcDial interaction on COM39 with active radio, preserving credentials/trust. Record any unavailable physical accessory gate explicitly.
- [ ] Check setup Back/Skip/recovery, saved location, offline embedded/missing-world fallback, legacy compatibility and regional PMTiles discovery/selection.
- [ ] Profile active-radio maps (frame/cache/PSRAM/largest DMA/SD/UI/USB/RF losses); distinguish embedded-world from regional SD activity.
- [ ] Build the final candidate, preserve recovery, flash COM39 app-only, and rerun impacted device checks. Document exact commit/artifact provenance.

## Task 4: Review and land

- [ ] Reconcile current status and limitations in the five `docs/maps/` deliverables without overwriting historical evidence.
- [ ] Review final diff against current origin/main, stage only intended source/docs/evidence, and run applicable CI gates.
- [ ] Open and attach PR, trigger CodeRabbit, run Grok review, verify and resolve valid findings.
- [ ] Merge only when all acceptance gates and both reviews finish successfully; verify merge commit on origin/main.

# LoRa worker startup with map memory retained

## Observed failure

COM17 (USB `30:ED:A0:E2:E6:95`), app `f61bdad5`, reported
`RTL_LORA_NATIVE_INIT_FAIL stage=task` on entry to LoRa. The driver still
streamed at about 955 ksps, with zero reported driver overruns/drops, but the
native decoder reported `ready=false busy=true` and history remained empty.
This is not evidence of successful decoding or of an antenna problem.

The user initially identified a powered GA-800 indoors beside a window, then
confirmed replacing it with a 915 MHz antenna before the controlled RF check.
COM24 (`44:1B:F6:6F:81:BC`) identifies as HELTEC_V4, node `!aa745cf5`, US
LongFast, automatic slot, no frequency override. COM17 uses US slot 20,
906.875 MHz. NODEINFO packet `3286217580` and text packet `711768941` were
submitted to the Heltec with hop limit zero and no acknowledgment request.
Neither was decoded during the bounded check. Submission is not proof of
over-air transmission. No radio settings, position settings or keys were changed.
Position sharing is disabled (`position_precision=0`) and no usable position
was returned, so no position packet was submitted.

## Narrow correction

The decoder requested a 12 KiB internal task stack while the observed largest
internal block was 7 KiB. Use the same PSRAM stack API already used by the
FT8, ADS-B and DSP workers, preserving stack size, priority and core. On startup
failure, delete the queue and clear handles. Automatic capture admission also
requires published decoder readiness, preventing enqueue before startup succeeds.
A queue left behind after failed task creation previously accepted one capture
and set busy with no worker to consume it.

Files: `apps/orcsdr-tab5/ui/main.cpp`,
`tests/lora_decoder_startup_test.py`, this evidence note.
No decoder algorithm, pack, SD file or receiver-location change.

## Checks completed before native build

From the integration worktree root:

```powershell
wsl -d Ubuntu -- python3 /mnt/f/AI/OrcSDR-TEMP/maps-integration-completion/tests/lora_decoder_startup_test.py
python .local/maps/check_setup_confirm_syntax_20261022.py
```

The startup harness compiles the actual production startup function with
fault-injected queue/task/workspace allocation. The old source fails the
post-task-failure cleanup assertion (`lora-startup-red-20261022.log`). Corrected
source passes initialization/workspace/queue/task failure, successful retry,
idempotence and PSRAM stack arguments (`lora-startup-host-final-20261022.log`).
The real P4 compiler syntax checks pass for main and setup UI
(`lora-startup-p4-syntax-20261022.log`). These do not prove hardware startup.

Raw device evidence: `lora-map-position-inventory-device-20261022.log`,
`lora-antenna-swap-pause-device-20261022.log`,
`lora-915-resume-device-20261022.log`,
`lora-915-packet-check-device-20261022.log`,
`lora-init-failure-stop-device-20261022.log`.
Transmitter inventory/submission: `heltec-map-test-{inventory,send}-20261022.log`.
All logs are saved in the invocation worktree root. COM17 reception was stopped
after the failed check. COM39 was not accessed.

## Native and controlled RF follow-up

Source commit `7c48e358981839e5750b05a6ff67976d6fe928e9` builds successfully:

```powershell
python .local/maps/build_fit_order_20261020.py build lora-worker-7c48e358-20261022
python .local/maps/inspect_lora_worker_20261022.py
python .local/maps/flash_lora_worker_20261022.py
```

The app is 5,139,616 bytes with 1,151,840 bytes of factory-partition headroom,
SHA-256 `bc03c76383befd3f5bbbfa8e027e85b37a6f077b83a0cb7436235fa1fbee9572`.
Inspection verifies the existing pin/symbol/PSRAM gates, diagnostics OFF and
unchanged 40 KiB DMA reserve. Flash verifies the prior app `f61bdad5`, backs up
and verifies non-app partitions, writes only the P4 app and verifies preservation.
C6, SD files, world partition, NVS, bootloader and partition table are preserved.
Boot reaches SD/Wi-Fi/RTL Ready with matching ELF hash. The LoRa task starts;
its initial reported stack high-water margin is 11,360 bytes.

The existing COM17 guarded serial harness ran these plans, all exit zero:

```powershell
python .local/maps/com17_label_bench.py lora-worker-boot-check-plan-20261022.json lora-worker-boot-check-device-20261022.log
python .local/maps/com17_label_bench.py lora-915-packet-check-plan-20261022.json lora-worker-rf-check-device-20261022.log
python .local/maps/heltec-map-test-send-20261022.py
python .local/maps/com17_label_bench.py lora-worker-retained-sd-plan-20261022.json lora-worker-retained-sd-device-20261022.log
python .local/maps/com17_label_bench.py lora-worker-real-position-controls-plan-20261022.json lora-worker-real-position-controls-device-20261022.log
```

The transmitter invocation saves `heltec-map-test-send-after-fix-20261022.log`.
NODEINFO ID `0x530172e7` and text ID `0xee0b32e8` exactly match CSV records,
ports 4 and 1, with `from_host=0`. Five CRC-valid packets were observed overall,
including two position reports from other nodes. One decode failure is reported
in the total background workload; do not describe this as an error-free RF run.
The controlled Heltec packets decode successfully. Their decode durations are
approximately 5.4–5.6 seconds, not map-frame timings.

Reception was stopped for the SD read; this is not simultaneous RF/SD acceptance.
The 881-byte CSV downloads with SHA-256
`771ac44d8a315cc6ae4ebde2e2d321e5b8b508d782fb319be8a350c3feca287f`.
The completed map frame remains 555,720 bytes, generation 2/revision 2 before and
after transfer; no map reclaim is reported. Readback memory is unchanged:
PSRAM free 652,684, largest block 507,904. Logging reports ready, no error,
queue empty and zero dropped log records. The evidence checker records exact RF
matches in `lora-worker-rf-evidence-check-20261022.log`.

With reception paused to keep ordering stable, serial selection of row 1 and
Center Node yield E7 `440139776,-1229717504`, exactly one received position.
Follow enables at that point, pan disables Follow, and Center Receiver restores
the saved receiver E7 `440628866,-1230160273`. Reception is then resumed on LoRa,
with the map open. This proves basic camera controls against received positions;
it does not prove stable identity across new packets or physical marker alignment.

Final observed PSRAM free 553,028 bytes, largest block 409,600; largest DMA
block 8,704; main-stack high-water margin 1,096; uptime 257,928 ms with no reset
after the intentional flash restart. The preceding resumed driver observation
reports about 950 ksps, zero driver drops/overruns. These are bounded results.
No Heltec configuration/key/position changes or COM39 operations occurred.

Build log: `fit-order-native-lora-worker-7c48e358-20261022-20261020.log`.
Inspection/flash logs: `lora-worker-{artifact-inspection,verify-previous,backup-*,verify-backups,app-write,preservation}-20261022.log`.
Native boot log, saved in its invocation directory:
`apps/orcsdr-tab5/lora-worker-boot-native-monitor-20261022.log`.
Recovery/artifacts: `.local/maps/lora-worker-7c48e358-20261022/`.
All monitor/test processes are closed.

## Remaining gates

Stable Follow identity needs a focused regression: selection still uses an index
against history reordered by reception. Physical overlay alignment, representative
33 ms cached interaction, global-pack/native quality, tight retained-memory/stack
margins, simultaneous RF/SD behavior and sustained stability remain open.
No correction or completion claim is made for these separate gates.

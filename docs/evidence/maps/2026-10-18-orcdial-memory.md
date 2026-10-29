# OrcDial fresh-boot pairing memory investigation

Status: source/host/native build and COM39 bridge-start/memory checks pass.
Complete OrcDial pairing/control delivery remains unverified. COM39's C6 must
remain unchanged, explicitly requested by the user; no C6 update was performed.

## Observed failure

COM39, serial E8:F6:0A:E2:D6:03, reports 7,379 DMA bytes free, largest
7,168, with the pairing bridge stopped and `failure=low_memory`.
The old guard requires 20,480 DMA bytes. An unchanged COM17 also refused pairing.
Logs are saved in the worktree invocation directory:
`stability-com39-orcdial-memory-20261018.log`,
`stability-com17-comparison-20261018.log`.

The normal startup capture `stability-com39-boot-20261018.log` shows DMA free
72,867 at boot, 34,003 after Wi-Fi, 16,055 at ready, 9,263 after starting RTL,
then 7,623 after background network startup. The separate RTC heap still has
about 31 KiB free. Restarting alone does not satisfy the existing pairing guard.

## Narrow fix

- Allocate the existing 12 KiB security and 4 KiB relay task stacks from internal
  RTC RAM via ESP-IDF's WithCaps task APIs. Keep the same stack sizes and core
  affinity. Do not move an NVS-writing task onto PSRAM.
- Call the existing reclaim registry when an explicit Pair/Connect request needs
  to start the bridge. Each holder remains responsible for refusing to free
  buffers it is using.
- Check separate DMA and RTC budgets after reclaim: DMA free >=6 KiB and largest
  block >=4 KiB, RTC free >=16 KiB and largest block >=12 KiB. These are startup
  admission checks, not a proof of sustained runtime headroom.
- Refuse PSRAM queue allocation failures without falling back into the DMA pool.
- Replace restart advice with a truthful insufficient-memory message.

The local ESP-IDF 5.5.4 P4 memory layout marks RTC RAM internal and byte accessible,
without DMA capability. The candidate config enables RTC fast memory as heap.
FreeRTOS checks both internal/byte accessibility for task metadata and valid stack
memory. SDK WithCaps creation frees its TCB/stack if creation fails.
References: [heap capabilities](https://docs.espressif.com/projects/esp-idf/en/v5.5/esp32p4/api-reference/system/mem_alloc.html),
[task allocation APIs](https://docs.espressif.com/projects/esp-idf/en/latest/esp32p4/api-reference/system/freertos_additions.html).

## Tests actually run

- `cmake --build .local/orcdial-security --config Debug` then
  `ctest --test-dir .local/orcdial-security -C Debug --output-on-failure`:
  6/6 pass, including P4 stack placement, repeated task-start failures and PSRAM
  queue failures without an internal allocation fallback.
  Log: `orcdial-memory-host-final-20261018.log`.
- WSL `python3 tools/test_orcdial_memory.py`: PASS with ASan/UBSan. Executes the
  actual production preparation function; checks reclaim, fresh-boot-sized
  admission, insufficient DMA/RTC and fragmented blocks.
  Log: `orcdial-memory-policy-host-20261018.log`.
- Real P4 syntax check: PASS, `orcdial-memory-p4-syntax-20261018.log`.
- The new P4 placement test failed on the original allocation path. Its first
  Windows assert opened a debug dialog; that test process was stopped. The
  expected failing check now returns an error to stderr without a dialog.
  Log: `orcdial-memory-host-red-console-20261018.log`.

## Native and COM39 result

Implementation commit `6f001d860012c80a07821e62c1658e89ff4a6109`.
Normal native build PASS; requested/locked/fetched engine and miniz symbol gates
PASS. App 5,029,568 bytes, SHA-256
`d0233366337139d0dfe0cd47e8872fdae2c8b5d4a9eb5738edcaa6609417e0b2`;
ELF `732b7ecdcbe874e440d4391456142d1bde996eb46e5c9a621c55ac23090ce104`.
Full 40,960-byte DMA reservation retained. Build log:
`orcdial-memory-native-build-20261018.log`; manifest:
`orcdial-memory-candidate-manifest-20261018.json`.

Fresh app/NVS/coredump/bootloader/partition-table backups are retained in
`.local/maps/com39-recovery/orcdial-memory-20261018/`. App-only write at 0x10000
passed flash verification. Bootloader, partition table, NVS, coredump and world
archive digests verified unchanged before boot. SD and C6 were not flashed.
Logs: `orcdial-memory-com39-backup-*-20261018.log`,
`orcdial-memory-com39-flash-20261018.log`,
`orcdial-memory-com39-preservation-20261018.log`.

Startup PASS: ELF identity matches, SD/Wi-Fi/RTL report ready. A pairing request
was already pending when the test opened Settings; the bridge started before
the script's expected `bridge=0` precondition. That script stopped on its stale
precondition. The follow-up observation completed successfully; it does not
claim the script's later automated Pair tap ran.

Observed preparation: reclaimed=0 (fresh boot), DMA free 7,435, RTC free 31,708,
RTC largest 30,720. After relay/security startup: DMA free 6,591, internal free
21,887, largest internal 14,848; `ORCDIAL_V4_BRIDGE_READY`. The device reports
bridge=1, Searching, failure=None. Subsequent samples: DMA free 6,415-6,635,
largest DMA 6,144, minimum unused security-task stack 9,212. This verifies startup
placement and searching, not peak authenticated crypto/control stack usage.
No reset/panic observed; sampled RTL driver drops/overruns remain zero.
Logs: `orcdial-memory-com39-boot-20261018.log`,
`orcdial-memory-com39-pair-20261018.log`,
`orcdial-memory-com39-observe-20261018.log`.

After cancelling the bounded search and stopping reception, SD readback of the
existing LoRa CSV passed transfer SHA verification: 56,227 bytes,
`4660d016e9b83acb7bcecbe9d7e3ec494a593de06a23bd8d131cda99409ab60d`.
Reception was restarted at the original 7,100,000 Hz. Final DMA free 6,579,
largest 6,144, security stack unused minimum 9,212. No SD file was overwritten.
Log: `orcdial-memory-com39-sd-20261018.log`; private readback:
`.local/maps/dial-memory-lora-readback.csv`.

## Remaining boundary and next milestone

COM39 reports host 3.0.6 / C6 1.4.1, match=0, and custom RPC request 388 times
out while searching. The user confirms this Tab5 cannot be used for the OrcDial
connection test and explicitly prohibits changing its C6. No updater command
was sent. The existing verified embedded C6 image was inspected only; it was
not installed. COM17 and OrcDial firmware were not updated.

The fresh-boot memory refusal is resolved in this bounded test. Retained-cache
reclamation is host-tested; live reclaim with populated caches, authenticated
control delivery, peak crypto stack and sustained coexistence remain unqualified.
The next connection milestone needs a compatible, authorized test device; do
not turn this result into a claim of complete pairing or all-mode stability.

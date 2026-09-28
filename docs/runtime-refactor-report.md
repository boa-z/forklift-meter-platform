# Runtime Refactor Report

Date: 2026-09-28. Branch: codex/runtime-production. This report separates implementation from remaining production qualification.

## Baseline and final identity

Baseline SHA: 3b8943a. Final firmware source SHA: 368eed1c346ec87912d4b2c40d366ca0cd1f915f. Later documentation-only commits do not change the tested image. Firmware version: runtime-production-j; Product: reference-demo; hardware compatibility key: reference-board.

SDK base SHA: 9b78386dcaa54326487c8b208b2b7104d4b56c9b. Reported SDK identity suffix: dirty-6a08b8bf070c2e33. No SDK source, persistent configuration or parent gitlink was edited. The existing parent working-tree status is retained. Restored configuration SHA256: 2494d4e32238755e8fe44f9bfc02cb034be6b2d1780fa2e87cc9ef653c3cf241.

Main changes: 4db6340 repairs Reference product contracts/fonts; 7cbf5a8 implements owners and semantic IPC; 338f7f0 adds native monotonic time and failed-TX backoff; ea048a4 preserves Product-selected command completion and session epochs; 368eed1 requires UI teardown acknowledgement before STOPPED. Changes are committed locally; this branch has not been pushed or merged.

Image: evidence/runtime-production/build-j, 1798656 bytes, SHA256 4512e27875634c2f3bed51e21ee4ccb5f4356e5ae3a4b43aec68cdaf8a0dca40. OTA: evidence/runtime-production/package-j/ota.cpio, 1098752 bytes, SHA256 ff7dca3f1c23beb0034a04f54844e150e3d65ee64903bf850189f72fe2c981d6. The package contains the exact built OS bytes plus native 4096-byte alignment padding. The build report's hardware_validation=NOT_RUN describes build-time status; subsequent physical results are recorded separately below.

## Architecture and ownership

Architecture: [Production runtime](runtime-production.md) defines typed ports, component ownership and runnables hosted by RT-Thread tasks. No ARXML, AUTOSAR OS or generic event bus is introduced. Core/common remain RTOS/LVGL independent.

Thread / runnable mapping: Protocol priority 19 / 8192-byte stack handles native RX semaphore wakeups, decode, CANopen, UDS and deadlines; App priority 20 / 8192 bytes owns Core, workflow, modes and publications; each CAN TX priority 18 / 2048 bytes owns blocking driver writes; UI priority 22 / 32768 bytes owns LVGL; NVM priority 24 / 4096 bytes and Update priority 25 / 12288 bytes retain blocking storage/Flash work. Components do not imply a dedicated thread each.

Ownership: Protocol stages semantic values and events without touching Core. App validates and applies them, arbitrates sources and publishes copied snapshots. UI consumes its own arrays and emits intentions. Debug/trace publication uses short copy locks; MSH formats outside those locks. Idle suspended workers are healthy waiting states, not a missed-heartbeat failure.

Mode model: App owns STARTUP, NORMAL, DEGRADED, UPDATE_MAINTENANCE and SHUTDOWN. Product declares capabilities. Rejection pressure enters DEGRADED; the reference recovery policy requires one second without another rejection. Generation changes invalidate old batches/TX, reset Protocol sessions and Product workflow, and stale old Domain values. Abort leaves explicit maintenance selection intact until maintenance off.

IPC contracts: 64 copied semantic batch slots, at most 32 values each; App validates the whole batch before any write. Events have 16 copied slots. UI intention/result and App command/result each retain one credit. QUEUED, APPLIED, TX_COMPLETED, REMOTE_CONFIRMED, FAILED and CANCELLED are distinct. Product selects the terminal success stage, defaulting to remote confirmation. Cancellation cannot undo a remote write already delivered.

Periodic scheduling: native monotonic milliseconds and planned deadlines avoid cumulative execution-time drift. Protocol drains at most 64 frames per pass with round-robin bus fairness, then yields; idle waits are 1 ms with UDS or 5 ms otherwise. Late periods are skipped without catch-up bursts. Mode changes explicitly rearm phase. The old eight-frame UI polling chain is removed from production.

TX architecture: each bus has 16 ordinary and 16 urgent slots, with at most four urgent frames before an ordinary opportunity. Only its TX worker calls the driver. Queue admission does not mean physical completion. Failed driver sends back off from 100 to 1000 ms; SDK HAL messages may still interleave UART output under no-ACK faults.

NVM integration: App owns revisions/requests/barriers; the existing worker owns EEPROM I/O and dual-slot writes. Maintenance rejects ordinary settings and permits required durability coordination. Shutdown rejects business requests, aborts Update, waits for durable NVM and worker acknowledgements, closes CAN, releases UI, then reports STOPPED. No forced thread deletion; overdue stopping remains observable. Restart after STOPPED requires reboot.

OTA integration: CAN -> Protocol/ISO-TP/UDS -> bounded Update jobs -> Update worker -> existing native OTA backend -> candidate -> NVM barrier -> activation/reboot. The duplicate OTA RX/TX owner is removed. Reference maintenance keeps critical 0x3C0, suppresses ordinary 0x2F0/telemetry and retains diagnostics/update UI. Hash is integrity checking, not a signature.

## Tests and measurements

Static analysis: strict portable compiler warnings and architecture/public-clean/ownership/documentation gates pass. Cppcheck OSS checked 14 portable first-party files without findings. Linux clang-tidy, ASan/UBSan and bounded libFuzzer jobs are configured but NOT_RUN here; fuzz harness strict syntax compilation passed. No formal MISRA or AUTOSAR conformance claim. Native IPC stubs test orchestration, not actual thread scheduling or freedom from data races.

Host tests: Demo UI 42/42; Reference-B UI 32/32; Reference-Mixed UI 31/31; optional OTA/headless 40/40. Python 96 passed, 11 skipped (eight opt-in physical cases and three environment-dependent package cases). Logs are evidence/runtime-production/*test-final.log and python-final.log. These counts overlap in shared gates; they are not 145 unique tests.

HIL: final firmware j, evidence/runtime-production/hil-j/20260928T114317Z-92e8e02d, eight tests passed in 29.43 s. Normal decode, boundaries, stale recovery, unknown ID, invalid DLC, unchanged Burst, periodic timing and NVM under load are covered. UART diagnostics and UI flush counters continue under load. Direct visual/touch confirmation for this final image remains pending.

Burst: original five frames per 10 ms over approximately two seconds, no lowered threshold. PC scheduled 1052 frames; native driver received 1052 with zero drops. Owner-published runtime delta was 1013 accepted/dispatched at its asynchronous sample boundary; the difference must not be mislabeled as proven packet loss or exact end-to-end delivery. Runtime overflow, native drops and error counters did not increase during this load window.

Periodic timing: six-second 500 frames/s input test, PCAN native timestamps. Results describe this run, not a vehicle-level timing guarantee. The test reports timing and long gaps; it does not invent a permitted jitter tolerance.

| Frame / period | Samples | Min spacing | Max spacing | Mean spacing | Max absolute deviation | Long gaps |
|---|---|---|---|---|---|---|
| 0x3C0 / 50 ms | 121 | 48.969 ms | 51.017 ms | 49.993 ms | 1.031 ms | 0 |
| 0x2F0 / 100 ms | 61 | 98.962 ms | 101.009 ms | 99.986 ms | 1.038 ms | 0 |

Protocol reported zero planned misses and maximum scheduler lateness 1 ms. Wire timing includes queue/driver/arbitration latency.

NVM coexistence: under 500 frames/s, brightness revision 20 became DURABLE, dirty=0, no storage error; UI flush advanced 350 -> 439. Runtime processed 5969 further frames with no additional native drop, queue overflow or CAN error. The fixture restored the original setting. Final shutdown test subsequently restored brightness 75 at durable revision 24.

OTA throughput: firmware i (ea048a4) installed final firmware j (368eed1) over 500 kbit/s with approximately 500 frames/s competing synthetic input: 1098752 bytes / 96.930 s = 11335.5 B/s (11.34 decimal KB/s). This is transfer through candidate-ready time, not total reboot duration. Candidate verification, activation, reboot and new meter info identity passed; NVM settings restored. Evidence: evidence/runtime-production/install-j. The full download executes the sender-side runtime i; final j separately passed HIL and active-download shutdown.

OTA mode test: 1024-byte Abort -> maintenance off -> NORMAL restored a fresh VALID speed signal; another maintenance session completed upgrade. During interior seconds 5-90 of the full transfer, capture contains 1700 critical 0x3C0 frames and zero ordinary 0x2F0 frames. OTA capture uses host arrival timestamps and is not precise wire-jitter evidence. OTA queue_rejected/tx_errors stayed zero; existing lifetime TX-rejection counters did not increase across this transfer.

Shutdown: final j, evidence/runtime-production/shutdown-j3. After 4096 bytes of active download with pending settings, cooperative stop completed in 2.768 s: Update ABORTED, durable revision 23, CAN closed, UI unavailable, new settings rejected. Owner threads exited; reboot restored brightness 70, then the probe restored brightness 75 durably. Earlier shutdown-j/j2 probe setup failures are retained: wrong expected state name and Abort against an already timed-out session; neither is reported as a firmware PASS.

## Resource measurements

Memory: final ELF text 1078252 bytes, data 12652 bytes, BSS 211552 bytes. This is linked static accounting, not peak total RAM including LVGL/native heaps. Semantic batch pool is 42752 bytes; TX owner structures including two stacks/queues total 8072 bytes. No common-layer runtime heap is added.

Thread stack high-water: RT-Thread integer percentage observations on final j before active-download shutdown: Protocol 25%, App 26%, TX0 35%, TX1 26%, UI 17%, NVM 23%, Update 16%. These are exercised peaks, not worst-case guarantees. Vendor software timer reached 81% of 512 bytes and needs margin review; no SDK stack was changed.

Queue high-water: final j connected post-boot load showed semantic batches 5/64, TX0 1 and TX full=0. Later HIL lifetime TX0 high=16/full=118 already existed before the measured load window and stayed unchanged during it; disconnecting the only ACK peer causes expected driver failures and bounded admission rejection. Batch full/rejected=0 and runtime overflow=0. Do not describe the entire boot as having zero TX rejections. Multi-bus fairness is implemented but not physically qualified.

## Known limitations and remaining production gates

Only one physical PCAN interface is connected. Simultaneous two-bus HIL with matching Mixed Product firmware remains open. Reference-Mixed/CANopen tests are Host evidence. The current board smoke build still selects the reference Demo; generic external firmware composition is not newly qualified by this refactor.

Run the new Linux quality job before merge; no remote CI PASS is claimed. Complete final visual/touch review, long-duration scheduler/fault stress, all Flash/backend shutdown phases, device-specific jitter limits and heap/stack worst-case measurements. The short HIL windows and source ownership gate cannot prove production safety or race freedom.

Native A/B candidate activation and new-image boot passed. Confirmation remains native_auto; rollback, power-loss recovery, signatures and anti-rollback are not newly verified. These results support integration review, not unrestricted production release.

# CAN HIL first board validation

## Baseline and evidence

The board name below is a public alias; original UART evidence retains the private identifier locally.

Date: 2026-09-28. Framework tested: 98b56a9. Board firmware Platform: 5aa2a20534c96975bfaaf31e8e42df6d6246ab50. SDK identity: 2bc652c45fa8a0aa352538666c98ea1f86a85ec3-dirty-0164123894516e10. lvgl-aic: dfdd4c0c07b6d09a438ca8a0627b3deeb3b0e918. Board: reference-board, LVGL 9.6.0, build Sep 28 2026 00:34:39.

The operator authorized PCAN_USBBUS1 and COM11 and released existing senders/terminals. The runner used python-can PCAN at 500000 bit/s and pySerial at 115200 baud. No flashing, reset, parameter change, PCAN-View GUI automation or customer data was involved. UART/CAN resources were released at the end; the original firmware remains installed.

Final raw evidence directory: evidence/hil/20260928T004353Z-be4481e1 (Git-ignored). metadata.json, junit.xml, serial.log, can.asc, identities, before/after diagnostics, boundary vectors, per-test Trace and automatic failure captures are retained there. An exact flashed-image hash was not supplied; image_sha256 is NOT_PROVIDED. Firmware identity was read from the physical board, not inferred from the current checkout.

## Host results

Local pytest: 20 passed, 6 hardware tests skipped by default. Existing Windows/MSYS2 CMake builds and CTest: Demo 32/32, Reference-B 23/23, Reference-Mixed 22/22. Architecture, public-clean and bilingual-doc gates passed. Linux CI now runs pytest before the unchanged three-product matrix; its final result must be checked on the pushed commit, separately from these local results.

## Physical results

| Gate | Result | Evidence |
|---|---|---|
| Normal decode | PASS | CAN/runtime counters increase; correct VALID speed/SOC |
| Speed/SOC boundaries | PASS | Ten deterministic vectors and Domain assertions |
| Stale/recovery | PASS | STALE then VALID; transition counters and signal Trace |
| Unknown ID | PASS | unrouted increases exactly once; normal data still works |
| Wrong DLC | PASS | decode_failed increases once, malformed unchanged; INVALID_FRAME Trace |
| Burst | FAIL | Native driver drops and insufficient frames reaching Runtime |

Overall verdict: HIL_FAIL, not HIL_PASS. Reference-Mixed HIL: NOT_RUN. Display/touch retain the user's earlier manual result; this run does not retest their appearance.

## Burst finding

Five messages were scheduled every 10ms. In the final measured window, Host scheduled 1080 TX attempts, native canstat received 1080 frames and reported 864 new receive drops. The meter Runtime snapshot observed only 192 new RX/dispatched frames. The elapsed test window including queries/teardown was about 2.259 seconds. The snapshots have different boundaries and do not form an exact queue balance equation.

meter can drop and runtime overflow remained zero. These counters describe the Runtime boundary and do not expose the native CAN receive FIFO drops. Thus zero in those fields is not evidence of a loss-free bus path. Host scheduling failure was ruled out for this window by the native driver's matching 1080 received frames.

Source inspection shows the Demo smoke loop calls meter_rtthread_adapter_poll with budget 8, then executes UI presentation/LVGL and sleeps 16ms. Receive draining therefore shares the UI cadence. This is a likely bottleneck consistent with the measured loss; changing the production thread architecture and measuring a rebuilt image is still required before claiming a verified fix. The test threshold was not reduced to turn this failure into a pass.

## Harness corrections and next gate

The first run caught the burst failure and automatically saved all five requested failure diagnostics. The second run added native driver evidence and exposed a UART fragment ending immediately after the uptime label; the parser had accepted it prematurely. Commit 98b56a9 now requires complete newline-terminated fields and numeric uptime, with a regression test. The final run has no UART teardown error and still reproduces the genuine burst failure.

Next: decouple Protocol reception from LVGL using native RT-Thread IPC and a single-writer Core design; expose native driver FIFO drop counters; build a new image and rerun the unchanged six HIL gates. Keep the generated height=6m float-range bug as a separate regression/fix. Do not claim Reference-Mixed PDO/SDO board coverage before its firmware and CAN TX port exist. See the [HIL workflow](can-hil.md) for reproduction commands.

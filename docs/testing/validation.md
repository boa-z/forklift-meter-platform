# Firmware validation record

## Delivered scope

Date: 2026-09-28. Baseline: ac3dc01. Original validation branch: codex/dynamic-periodic-tx. Firmware source: 3c141ff479e40a2b7d21f2873d7ccb0c05b939aa. Host UART tooling: 4793bb48ff72056a2a1e18352d6287bccb693c51. The board runs dynamic-tx-e. No SDK source, persistent configuration or parent gitlink changes were made.

Current contracts are in [Runtime](../runtime/runtime-production.md), [Dynamic TX](../runtime/dynamic-periodic-tx.md) and [governance](../compliance/status.md). This page retains historical test identities: branch names and original SHAs do not imply that current main and installed firmware have identical identities. Superseded phase reports remain in Git history.

## Validation

Local CTest: Demo 44/44, Reference-B 34/34, Reference-Mixed 33/33, headless OTA 42/42. Python: 97 passed, 12 skipped; physical HIL is separately executed. Linux host and quality jobs passed at firmware source 3c141ff (run 36430288416) and Host-tool source 4793bb4 (run 36430815503). Firmware-source quality evidence includes ASan/UBSan 42/42, original analyzers, seven generated production translation units checked by two analyzers, and 423074 fuzz executions in 31 seconds. No thresholds or checks were weakened.

Final single-bus HIL: 9/9 passed in 30.47 seconds. A 500 fps load/burst exercise captured 1052 scheduled and 1052 native driver RX frames with no driver drops or runtime overflow; dispatched counts are measured over their own observation window. NVM/UI coexistence, freshness stop/recovery, dynamic version selection and existing runtime regressions passed. 249 dynamic TX frames passed byte complement/XOR checks. Brightness versions 31/47/63 used revisions 20/21/22 at their first permitted deadlines, respectively 22/11/7 ms after acquisition.

| Measurement | Critical 50 ms frame | Ordinary 100 ms frame |
|---|---|---|
| Interval samples | 125 | 62 |
| Mean interval | 49.9971129 ms | 99.9985246 ms |
| Minimum interval | 49.195 ms | 99.201 ms |
| Maximum interval | 50.987 ms | 101.036 ms |
| Maximum absolute jitter | 0.987 ms | 1.036 ms |
| Long gaps | 0 | 0 |

Scheduler lateness, queue residence and driver duration at the selected first-deadline samples were each 0 ms at 1 ms diagnostic resolution. This does not mean zero physical latency. PCAN RX timestamps preserve their native clock; Host TX wall time has a different origin. Correlate identities and ordered log windows, and calculate RX intervals within the native clock. Do not subtract the two clock origins. These measurements are observations, not new timing requirements.

## NVM and OTA regression

The original NVM priority 24 could delay repeated EEPROM I/O wakeups behind UI priority 22. Moving NVM to priority 21 keeps App/Protocol/TX ahead of it. An observed single commit improved from 8.951 s to 3.865 s without changing byte-write/readback behavior or persistence layout. The existing 12 s HIL threshold remains unchanged. Final read-only verification found DURABLE, dirty=0, errors=0 and matching RAM/durable revision 54; current brightness 10 was retained.

OTA dynamic-tx-d to dynamic-tx-e passed under 500 fps background traffic: 1098752 bytes in 101.4973777 s, or 10825.42 B/s (10.83 kB/s). A 1024-byte abort returned to normal mode; the complete transfer then installed, activated, rebooted and confirmed the new firmware. Brightness 75, language 1 and imperial 1 were preserved across the upgrade. A bounded maintenance window contained 1655 critical frames and zero ordinary frames; critical interval mean was 50.000049 ms, range 49.067020–50.987005 ms. Historical no-ACK errors between disconnected test sessions are not represented as lifetime-zero errors.

## Artifact identity and evidence

| Artifact | Identity |
|---|---|
| Test image, build-e/*.img | SHA256 1c1481f1a8996876f47eb1e99010a032c7754fba72f0852295fbebc215f862ab |
| os.itb | SHA256 21f984729b04c6cff47c9c2c3e6f4d868e96c1eef010c45f511e89cffe61ed46 |
| package-e/ota.cpio | SHA256 5da9af4fe5190c3f69fe7a7b6199d4e16330946095fd6753203550ccdfe76cf6 |
| Restored SDK .config | SHA256 2494d4e32238755e8fe44f9bfc02cb034be6b2d1780fa2e87cc9ef653c3cf241 |

Local evidence under evidence/dynamic-tx/: build-e, package-e, install-e-retry, hil-e/20260928T134536Z-8b4376dc, final-state and ci-quality. Raw CAN/UART logs, JSON diagnostics, JUnit and failed preliminary attempts are retained locally; they are not public repository fixtures. Final image size is 1800704 bytes; ELF text/data/bss are 1082908/12716/214560 bytes. The incremental target capture contains 47 actual compilation commands, not a complete target compilation database.

## Governance and limitations

Tool Quality Green is supported by the exact CI runs above. Project Governance Conforming remains PENDING: TAD-001 needs human applicability approval and main required-check enforcement is absent (protection HTTP 404, rulesets empty at inspection). Formal MISRA Compliance is NOT ASSESSED. Adopted target LVGL adapter license evidence also needs review. No agent approval is implied.

Original portable analyzers cover core/runtime/storage/update with the contracts header filter; additional analysis covers seven generated sources. Handwritten Product/platform/UI translation units and target-specific analysis are not comprehensively covered. Target GCC is Xuantie 10.2.0, newlib 3.2.0, rv32imafdcpzpsfoperand_xtheade/ilp32d, O2; no explicit C standard flag was captured. Linux tool versions and dependency pins are recorded separately.

Catalog field ordering changed to eliminate padding findings; generated and in-repository initializers now use designated fields. External Products using positional initializers must migrate and rebuild. Explicit persisted record encoding is unchanged. The first periodic contract uses a uint32 wire-state token; diagnostic identities and wire identities expose bounded low bits while internal revision remains 64-bit.

Only one physical CAN interface was exercised. Simultaneous multi-bus behavior, rollback, secure update and power-loss recovery are not newly claimed. Remaining gates are human applicability/license review, administrator required-check configuration, measured target analyzer expansion and a multi-bus fixture. No new Demo pages or SDK refactor were added.

## Earlier shutdown validation

Firmware runtime-production-j (368eed1c346ec87912d4b2c40d366ca0cd1f915f) cooperatively stopped during a 4096-byte download with settings pending: in 2.768 s Update became ABORTED, NVM reached durable revision 23, CAN closed, UI released and new settings were rejected. Reboot restored brightness 70, then the fixture restored 75. Evidence: evidence/runtime-production/shutdown-j3. This belongs to that historical image and is not relabeled as a complete shutdown fault matrix for dynamic-tx-e.

## Application services board validation

Source 5cfa0bfeefa7e97dccb56319986af7704673a9bf, firmware services-a, exactly one Demo Product. Tested on the connected board with public identity reference-board, COM11 and PCAN_USBBUS1 at 500 kbit/s; the SDK handover record identifies the physical target. Run 20260928T171051Z-dd46a0a2 is UTC September 28 / local September 29. This is new physical evidence, not reuse of the older dynamic-tx-e results.

| Check | Result / scope |
|---|---|
| Native SCons build | PASS; Xuantie GCC 10.2.0 V2.6.1, SDK Python 3.8.10; original configuration restored byte-for-byte |
| Build-time composition | Demo composition and projection linked; no Reference-B/Mixed sources selected; alternatives remain separate host builds |
| Existing CAN OTA | Validated 1102848-byte package, activated and rebooted; UART and CAN identity match source 5cfa0bf / services-a |
| Physical HIL | 9/9 PASS; unchanged gates cover receive/decode/stale recovery, unknown ID, DLC rejection, burst load, periodic timing, NVM under load and dynamic publication |
| Burst observation | Native RX 1051, native drop delta 0; Domain path RX/dispatched 1015; no new gate-counted errors/overflow/reset |
| Periodic observation | 50 ms mean 49.9935, maximum absolute jitter 0.9420 ms; 100 ms mean 99.9878, jitter 1.0740 ms; zero observed long gaps |
| Handover | RUNNING/NORMAL; update IDLE, maintenance off; EEPROM DURABLE, dirty 0, RAM/durable revision 60, brightness restored to 10; UART/CAN released |

Periodic numbers describe one measured window, not a vehicle tolerance or worst-case guarantee. Lifetime CAN errors include intervals without a Host ACK endpoint; HIL assertions use unchanged window deltas. UI flush progress was observed, but no visual/touch acceptance or photograph is claimed. New calibration/profile mappings remain opt-in synthetic tests, not real-controller calibration. Power-loss, multi-bus and private Product integration are NOT_RUN.

| Artifact | SHA256 |
|---|---|
| Full build-a image, not installed as a whole | 276fb0dc4fcba9e92ab7ffa132cf09de2d076990a1c0a6b2aa0731956c427426 |
| Built d13x_os.itb | 9137f6a0178b12fc2282e6469d84e8b589e815499f27b035b79c6276bc2d8fcd |
| Installed package-a/ota.cpio | 3ce1457a2d318c2cebb8c00d248961bc4d9c9470b12821e5f419aa95c750eb70 |
| Restored SDK .config | 2494d4e32238755e8fe44f9bfc02cb034be6b2d1780fa2e87cc9ef653c3cf241 |

Evidence root is evidence/services/: build-a, package-a, install-a, hil/20260928T171051Z-dd46a0a2 and handover. sha256.json indexes raw UART/CAN, JUnit, configuration, image and JSON records. HIL image_sha256 denotes the actually installed CPIO package, not the full image. OTA changed the OS slot through the existing native auto-confirm path; bootloader and trust policy were not changed.

SDK identity is 57777e0b5d027e030de1daf0799f0c05102953d9-dirty-378865321b7708a1; its patch/status and exact dependency pins are archived. LVGL is 9.6.0, lvgl-aic dfdd4c0c07b6d09a438ca8a0627b3deeb3b0e918. Recursive dependency inventory exposes an inherited iso14229 documentation submodule with missing .gitmodules mapping; partial error and nonrecursive pins are retained, not silently repaired. Native capture includes 71 actual incremental compilation commands, not a complete target database. ELF text/data/bss are 1083436/12716/214624 bytes. Existing SDK probe/pywin32 warnings remain in the raw build log. This does not establish complete target analyzer or current-head Linux CI acceptance.

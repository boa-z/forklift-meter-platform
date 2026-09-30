# NVM persistence

## Ownership and policy

App owns RAM settings and the NVM revision service. The RT-Thread worker owns EEPROM/file I/O and its slot cache. Commands/results use native single-entry message queues; only one immutable payload is in flight. An additional pending buffer retains newer settings. The SDL host uses the same record/service/backend code in a dedicated thread. Neither worker reads live Core. No storage I/O occurs in a UI action callback.

Reference-Demo explicitly declares namespace 0x444D, record type 1, schema 3, 500 ms debounce and a 3000 ms maximum unsaved interval. Its synthetic local parameters plus language, units, brightness and restart-effective CAN rate form one complete replacement record. External Products must opt in with their own namespace/schema and local-authority policy; remote ECU parameters must not be made authoritative by copying this policy. Storage revision is independent of telemetry/Domain revision. Equal payloads do not increment it.

## Encoding and commit

FMP2 is little endian. The MSP3 payload uses stable 32-bit encoded parameter IDs (restricted to the Domain uint16 range) and IEEE binary32 values, not catalog positions. Missing, repeated, unknown or out-of-range values reject the whole payload. The inner FNV checksum detects buffer corruption; it is not authentication. The outer CRC uses the pinned CANopenNode CRC-16/XMODEM implementation (polynomial 0x1021, initial 0, standard check 0x31C3). Header and payload are chained, not XORed independent CRCs. Golden bytes and single-byte corruption checks are in test_meter_record.c.

| Offset | Size | Field |
|---|---|---|
| 0 | 4 | FMP2 magic |
| 4 | 2 | Version 2 and zero reserved byte |
| 6 | 2 | Record type |
| 8 | 2 | Product namespace |
| 10 | 2 | Schema |
| 12 | 2 | Flags, currently zero |
| 14 | 8 | Monotonic commit sequence |
| 22 | 4 | Payload length |
| 26 | 2 | CRC of bytes 0–25 followed by payload |
| 28 | 4 | Reserved, must be zero |
| 32 | variable | MSP3 payload |
| tail | 16 | Sequence, payload length, CRC and complemented CRC |

Each slot also has a separate commit page holding a copy of the bound 16-byte seal. Commit invalidates this seal, syncs and reads back; writes the complete body, syncs and reads back; writes the bound seal, syncs and reads back. Only then does the active slot change. The last valid slot is never written. Scan distinguishes EMPTY (all 0xFF), VALID, CORRUPT, INCOMPATIBLE, I/O failure and conflicting equal sequences. One damaged and one valid slot yields degraded recovery. Unknown schemas prohibit automatic overwrite. No legacy migration, implicit format, silent medium fallback or sequence wrap is provided.

## Backends

| Backend | Integration | Guarantee and boundary |
|---|---|---|
| reference-board EEPROM | BL24C512A family, i2c0, 0x50, 64 KiB, 128-byte pages | Authorized new allocation 0x0000–0x03FF; A at 0x0000, B at 0x0200; seal pages 0x0180 and 0x0380. Initialization, save and soft reboot verified; physical power cuts unverified. |
| Host file | Prefix.0 and Prefix.1 | Complete writes, fsync/_commit and close checked. Readback required. Filesystem metadata crash atomicity not claimed. |
| NOR/LittleFS file | Existing mounted SDK filesystem | Same selectable file adapter; no raw NOR driver or format operation. Device sync/XIP and physical recovery require Board validation. |
| NAND/FATFS file | Existing SDK NFTL/ECC and mounted filesystem | Weaker filesystem guarantee; dual files do not prove independent metadata redundancy. Critical power-safe capability requests are rejected. |

The application-only EEPROM transport uses existing RT-Thread I2C APIs without changing the SDK, HAL, board configuration or parent gitlink. The reference-board compatibility path sends a two-byte address and receives one byte in separate single-message transactions. The recursive RT-Thread bus mutex protects each address/read pair. The worker waits 1 ms before each transaction pair and before each byte write. It also waits 1 ms between the address and read while retaining that mutex. A short address/read transfer retries the complete read pair at most three times, logging each failure; failed attempts never publish their output byte. Mutex errors do not retry. Writes are never automatically replayed. Page callbacks split into three-byte single-byte writes, wait 10 ms outside the bus lock after every attempted write, and read back each byte before continuing. A short write, exhausted read retries or mismatched readback stops the operation; a driver success alone is not durability. Generic page bounds, read-only readiness checks and full record/seal readback remain mandatory. No destructive SDK AT24 probe is used. This conservative mode sacrifices throughput: 1 KiB initialization needs at least 10.24 seconds of write-cycle waits, plus driver/readback time; allow 60 seconds in board validation. Driver behavior remains governed by the unchanged SDK; the validated hardware scope is recorded below. A commit seal shares no body page or active-slot page.

AIC_FORKLIFT_NVM_FILE_BACKEND explicitly selects the mounted file path; AIC_FORKLIFT_NVM_FILE_PREFIX and AIC_FORKLIFT_NVM_FILE_KIND identify it. Default reference-board composition uses EEPROM. A mount failure never formats the medium or switches to another backend. The first EEPROM image must only run after the new 1 KiB allocation and WP are confirmed; existing nonblank unknown content is preserved and reported.

## Results and lifecycle

LOADING is an asynchronous startup operation. App accepts the matching generation before settings actions are enabled. Accepted defaults on a blank preference region become dirty and are initialized through the ordinary reliable commit. Invalid records leave RAM defaults usable but writes blocked; a query does not initialize storage.

IN_PROGRESS owns the immutable in-flight revision until a matching result arrives. Completing R11 while R12 is pending advances durable to R11 and leaves R12 dirty. An I/O-stage failure is UNCERTAIN, preserves dirty, and requires explicit rescan/reconciliation before retry. Reconciliation never replaces newer RAM with media data. A barrier waits for a particular target revision; a higher complete replacement may satisfy it, but an intermediate value is not claimed to have been written exactly. A loaded valid record satisfies its target. Cancellation is only supported before worker dispatch. No blocking worker is killed or freed on timeout.

Host shutdown requests immediate save and waits a bounded interval. Failure produces a failing process exit and preserves dirty semantics. Board callers use meter_board_nvm_flush and meter_board_nvm_barrier from App; OTA must request this through App before activation. Current Protocol/App/UI scheduling remains the existing smoke arrangement; this NVM change isolates storage I/O but does not close the separate production-runtime/Burst gate.

## Diagnostics and validation

Use the following native MSH commands. Each mutation must have its result collected before another serial command is accepted; QUEUED/APPLIED does not mean DURABLE.

~~~text
meter storage
meter_settings language zh
meter_settings result
meter_settings units imperial
meter_settings result
meter_settings brightness 65
meter_settings result
meter_settings save
meter_settings result
meter storage
meter_settings retry
meter_settings result
~~~

Storage reports backend, state, dirty, RAM/in-flight/durable revisions, queue depth, raw backend error, service result, degraded recovery and actual language/brightness/units. Queries render published copies and never access media. Poll DURABLE and dirty=0 before an intentional reboot; after reboot verify settings and sequence. The save target is distinct from the asynchronous command result.

Host covers every byte cut while reusing an inactive slot, identity/schema/CRC/seal corruption, conflicting sequences, queued supersession, stale completion, time wrap, finite debounce, revision exhaustion, cancellation, reconciliation, EEPROM page splitting/WP/NACK/timeout/sync failure and real file restart. Demo SDL restart tests persist English/Chinese and units, and reject damaged slots without rewriting them. Run CMake/CTest; Linux remains the CI target.

Current acceptance on 2026-09-28: BOARD_INIT, SETTINGS_SAVE, NOOP_SAVE, VALID_INIT_REJECT, SOFT_REBOOT_RESTORE and NORMAL_CAN_SAVE are PASS on the application-only bounded-read recovery image. Platform commit is 4fca8762243f9bb40cb753abd553f0808865cce0 with the recorded dirty-source snapshot; image SHA256 is bf5ef8efa339376ffe83bd526201f0e62f7b30ff69c85a5608bf534a39ce3635. SDK commit 9b78386dcaa54326487c8b208b2b7104d4b56c9b has the same tracked tree as baseline 7608a666. Local image manifests retain exact build identity, source snapshots and raw UART/CAN evidence; public documentation uses the reference-board alias.

Initialization plus the first durable default record took 55.7 seconds as observed through polling. One address transfer at 0x0207 failed on attempt 1/3 and recovered within the bounded read retry. The initial cumulative error count reflected the old corrupt layout; the final backend error was zero. Saving Chinese, imperial units and brightness 65 reached durable revision 4 in 15.3 seconds from the first setting request, including two commits for successive revisions. Repeating the same values and save request caused no revision or write increase. Initialization with a valid record was rejected.

After an intentional software reboot, the same settings and revision 4 loaded with READY, dirty=0, depth=0, errors=0 and writes=0. READY means a valid record loaded at startup; it already satisfies its durable-revision barrier. DURABLE marks a successful runtime commit and is not the required startup state. The first harness assertion incorrectly required DURABLE; its original failure report is retained beside the source-validated correction. A Host regression covers READY, matching payload/revisions, the barrier and no-op save without a new write.

With the existing normal CAN scenario at a nominal 50 frames/s, two further saves reached durable revisions 5 and 6 in observed 9.4 and 6.9 seconds. Host scheduled, native received, application received and runtime dispatched counts all matched at 890; native drops, application errors and runtime overflow were zero. Speed remained VALID, UART diagnostics responded and UI present/flush counters progressed. These observations do not establish physical touch or rendered UI acceptance. Final settings are Chinese, imperial units and brightness 65; COM and PCAN resources were released.

Cold power-cycle recovery is PASS after the user removed and restored power: firmware identity matched, revision 6 and Chinese/imperial/brightness 65 restored with READY, dirty=0, errors=0 and writes=0. Power interruption during a write and endurance remain NOT_RUN. The byte transport is conservative and slow; elapsed values include command/polling overhead and are not raw EEPROM throughput. Existing CAN Burst FAIL remains unchanged, and this normal-load test does not close the production-runtime or full HIL gate. Host mocks alone do not establish hardware recovery. This is engineering conformance work, not licensed MISRA certification.

Historical failures remain recorded: the earlier byte-transport image (SHA256 0cf247370a4c9ba344435919a8df0b1f144d5eb70042b1f70a23ba96c2ef887e) failed initialization readback at 0x003C and rescan at 0x0089. The experimental SDK I2C repair was fully reverted and its image withdrawn. The current application-only settling and read retry path passes the scoped board tests above; it is not a claim that the SDK controller implementation was repaired.

For commissioning an old layout, export a backup and obtain layout authorization before meter_settings initialize CONFIRM. It requires no valid slot and a successful previous I/O. Writes, sync and readback run in the NVM worker; a valid slot cannot be erased. Collect meter_settings result, then verify DURABLE with meter storage; queue acceptance is not completion.


## Board settings and format boundary

MSP3 bytes 0..3 hold magic, 4/5/6 hold units/brightness/language, 7 holds the CAN rate selector (0=125k, 1=250k, 2=500k), 8..11 hold parameter count, ID/value entries start at 12, and the last four bytes hold FNV. Only MSP3 is supported. Demo declares FMP2 schema 3; no legacy reader or migration is provided, as requested by the maintainer. Old/corrupt records are rejected atomically and remain write-blocked until explicit initialization. Initializing the development-board allocation was authorized; automatic erase on boot is not introduced.

Reference-board startup waits for the initial NVM read before latching the common rate for both CAN devices. Runtime changes save only and do not reconfigure controllers. Corrupt records retain the default 500k and write-block policy. configured_can_rate in meter storage is the next-boot configuration; bitrate in meter can is the opened controller rate. Before restarting confirm dirty=0 and ram_revision=durable_revision.

App applies brightness immediately through the Board PWM adapter and saves it through the same NVM service. Register-readback failure logs an error and retries; RAM acceptance proves neither successful backlight control nor storage durability. meter_settings product id value queues a diagnostic intent for the App Product callback, including administrator authorization; the generic port does not interpret Product-private IDs.

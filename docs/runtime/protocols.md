# Protocol and Domain contracts

## Domain and publication revisions

Core revision compares complete meter_value_t values, including timestamp, state, source and value; parameter, fault, connection and settings changes also update Core revision. This differs from Dynamic TX semantic revision, which advances only for semantic value/validity changes: a same-value sample may update sample time alone. A signal stale_ms of zero disables automatic timeout, with no global fallback. Source arbitration receives complete current/incoming values. See [dynamic TX](dynamic-periodic-tx.md).

## Protocol service boundary

### Current API

Adapters expose one on_frame/process/command/reset interface. meter_protocol_services_t carries Domain update, instantaneous Event and CAN TX with separate callback contexts. Application submits business commands through meter_runtime_command; Product routing selects the adapter. Temporary owner APIs and legacy dual callbacks are removed.

### Transactions

Signals are continuous state, Events instantaneous, Commands business requests, Transactions request lifecycles. CANopen SDO transactions belong to the standalone scheduler and upstream client. Runtime has no singleton transaction or transport timeout. The old generic pool was removed after its only production consumer migrated.

### Boundary

Product workflows consume read/write results outside transport callbacks. Core knows no PDO/SDO; UI knows no CAN; CANopenNode contains no customer business; Runtime has no product branches. Protocol calls are serialized and thread boundaries use native RT-Thread IPC.

## CANopenNode dependency and capabilities

### Dependency

CANopenNode is pinned to v4.1, ac2140717c3c498d9b0351bce052bab630a74764, under third_party/CANopenNode (Apache-2.0, no local patches). Its purpose is the module-level SDO Client protocol engine.

### Selection

Products declare canopennode-sdo in the features array of product/sources.json. METER_ENABLE_CANOPENNODE enables the standalone target; Demo defaults to OFF. Products requiring SDO reject explicit OFF. Fixed PDO binding remains independent. The capability profile describes selectable features, not a global ban on NMT/Heartbeat for other products.

### Verification

Reference-Mixed selects PDO RX/TX and SDO Client without NMT/Heartbeat. Binary tests prove isolation and host tests exercise real upstream modules. Full CO_t lifecycle, EMCY/SYNC and hardware acceptance are outside this SDO integration.

## CANopenNode module-level SDO integration

### Design correction

The complete CO_t/CO_process lifecycle does not fit this PDO/SDO subset. Standalone CO_SDOclient_t does: it needs a static CAN driver bridge and a 0x1280 client parameter record, without CO_new, heap allocation, NMT or Heartbeat. The previous contrary conclusion has been removed. Fixed PDOs retain static Platform binding into Domain. CANopenNode v4.1 (ac2140717c3c498d9b0351bce052bab630a74764) owns all SDO framing, expedited/segmented state machines, toggle validation and protocol timeouts; the upstream submodule is unmodified.

### Build and ownership

Production compiles only upstream 301/CO_SDOclient.c, 301/CO_fifo.c and 301/CO_ODinterface.c. Segmented and FIFO are enabled; block, local transfer and dynamic OD are disabled. All bridge storage is static. The minimal OD contains only 0x1280 subindices 0 through 3; requests select remote nodes through CO_SDOclient_setup. Product manifests declare canopennode-sdo. CMake enables its target and rejects a conflicting explicit OFF. Demo/Reference-B never link the client. CO_SDOserver_t is a host-only test peer, not a product/firmware dependency. No CANopen.c, NMT or Heartbeat source is compiled.

### Scheduler and transport

A channel has four retained request/result slots, one active transfer, FIFO ordering and a 128-byte payload limit. Request IDs are independent of OD index/subindex and unique among occupied slots. Products submit READ/WRITE. Results remain available until taken: PENDING, SUCCESS, ABORTED, TIMEOUT. Reset cancels active/queued work with observable ABORTED results.

CANopenNode owns each transfer timeout; the scheduler owns bounded retries, delay and optional retry-on-abort (off by default). Write retries preserve the original operation and bytes. The generic transaction pool was removed because it had no non-SDO production user. Runtime maintains no second timeout.

A false send retains the original upstream TX buffer for later flush; bufferFull prevents overwrite. After upstream timeout, an unsent obsolete request is explicitly canceled to let upstream send abort. A buffered abort must flush before the next request/retry starts. Reset explicitly cancels buffered transport data.

### Thread boundary

All bridge, scheduler and receive calls run serially on one Protocol thread. ISR/other threads must deliver frames via RT-Thread native IPC. No-op critical-section macros depend on this ownership contract; direct concurrent ISR calls are unsupported. App/Core remains a separate single writer; UI never accesses CANopenNode. Production ownership is defined in runtime-production.md; Host tests do not prove native scheduling.

### Verification

Real upstream client/server tests cover expedited 1/2/4-byte reads/writes, standard 0x2B two-byte writes, 17/128-byte segmented transfers, abort codes, oversized reads, timeout, read/write retries, wrong/late responses, reset/reconnect, TX busy and queue-full behavior. Mixed tests exercise valid-PDO-gated startup, failure without false READY, command routing and PDO freshness recovery.

The binary gate requires CO_SDOclient symbols in the selected product and SDO archive; it rejects CO_new, CO_process, CO_NMT*, CO_HBconsumer* and CO_SDOserver* in products, plus malloc/calloc/realloc/free definitions or references in the SDO archive. Host executable CRT allocation is outside the module no-heap boundary. Demo/Reference-B executables must contain no SDO client symbols.

### Limits

SDO frames have no application request ID. Idle/retry-wait responses, malformed frames and wrong COB-IDs are rejected; upstream handles wrong object responses. A delayed identical-object response or old segment with the same toggle after a new transfer starts cannot be reliably distinguished. Products must bound latency, choose a suitable retry quiet time or reset before reuse. Local reset cannot undo a remote write already accepted.

FIFO/payload limit is 128 bytes; block mode is unsupported. No NMT/Heartbeat traffic is generated. Hardware bus-off, electrical behavior and board acceptance require separate tests. Host test peers do not prove firmware or board acceptance.

## Reference-Mixed service migration

### Composition

CAN0 retains the synthetic DBC/codec. CAN1 retains RPDO 0x20C static Domain binding and TPDO 0x18C; the product UI is unchanged. SDO 0x60C/0x58C uses the standalone client/scheduler. NMT/Heartbeat stay disabled and PDO freshness supplies communication state.

### Workflow

StartupParameterSyncService in services/startup_parameter_sync.c waits for a valid PDO, reads 0x2000:01, requires success, then reads 0x2000:02; only both successes enter READY. Values live in product service state. It interprets parameter payloads, never CAN frame bytes. Terminal failure enters FAILED and emits an event; PDO loss cancels work and returns to WAIT_DATA; recovery restarts synchronization.

Application commands pass through meter_runtime_command, Product routing, Adapter and scheduler. Two-byte writes use upstream 0x2B. Invalid commands are rejected. Request IDs are independent sequence numbers. Reset clears product workflow/channel state.

### Tests

mixed-domain preserves DBC replay/stale tests. mixed-canopen uses the upstream host server for WAIT_DATA, A/B ordering and values, both parameter writes, abort/retry exhaustion without false READY, disconnect/reset and PDO timeout/recovery. Transport/segmented coverage is in canopennode-sdo; symbol isolation is in canopen-binary. Architecture, public-clean and public-header gates remain enabled.

### Board boundary

Reference-Mixed Product/UI, PDO/SDO and startup service have Host coverage; single-bus Demo board results do not establish Mixed dual-bus acceptance. Select the matching Product firmware and validate CAN1, transport ownership and physical wiring.


## Demo instrument transmit PDOs

The single Demo Product additionally publishes two public synthetic standard CAN0 data frames at 100 ms, DLC 8, little endian. The transmit-only definition is `products/demo/protocol/can/demo_tx.dbc`; the receive `demo.dbc`, its generated adapter and routes remain unchanged. These examples do not implement CANopen NMT, SYNC, heartbeat, configurable PDO mapping or an object dictionary. They are not customer protocol definitions or vehicle control commands.

| Frame | Bytes / bits | Meaning |
|---|---|---|
| 0x381 demo_tpdo_motion | 0–1 / 2–3 / 4–5 / 6 | Speed 0.01 km/h / lift height 0.001 m / load kg / SOC percent |
| 0x381 demo_tpdo_motion | byte 7 bit 0 / bits 1–7 | Whole-group fresh / 7-bit sequence |
| 0x481 demo_tpdo_status | byte 0 bits 0–4 | Seat occupied / brake active / neutral / charging / warning |
| 0x481 demo_tpdo_status | byte 1 bit 0 / byte 2 / byte 3 | Whole-group fresh / 8-bit sequence / layout version 1 |
| 0x481 demo_tpdo_status | bytes 4–7 | Runtime session generation, not a firmware version or persistent counter |

Unused bits are zero. Motion depends on speed, SOC, height and load; status depends on the five boolean source values. App preserves their source timestamps. Any unavailable/out-of-range source or age >= 500 ms makes that group invalid (Domain stale policy can invalidate earlier). An invalid group sends zero measurements/status flags and fresh=0; valid zero remains fresh=1. Receivers must inspect fresh before using values. Counters and layout/session metadata remain available when invalid. Normalized boolean inputs must be exactly 0 or 1. Floats are range-checked before conversion and scaled toward zero, matching the existing speed encoder.

Both new frames are ordinary traffic, suppressed in update maintenance. Sequence wraps modulo 128/256 and advances on successful local driver completion, not remote acknowledgement; it restarts on runtime generation changes. Do not interpret a counter or status flag as a safety guarantee. The legacy 0x3C0 and 0x2F0 payloads, dependencies, periods and commit policies remain intact. All frames share the existing publication revision; additional PDO semantic changes can therefore advance the revision also carried in the legacy frames.

`demo-pdo` tests source validity, known wire vectors, valid zero, freshness boundaries, sampling timestamps, driver completion and static budgets. `demo-pdo-dbc` decodes actual C output with cantools. Physical `test_demo_instrument_pdo` checks payloads, incoming-data loss/recovery and wire sequence using PCAN and the transmit DBC; Host success alone does not prove board transmission. Real Products must choose confirmed mappings and policy independently.

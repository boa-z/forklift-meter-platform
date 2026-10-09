# Protocol and Domain contracts

## Domain and publication revisions

Core revision compares complete meter_value_t values, including timestamp, state, source and value; parameter, fault, connection and settings changes also update Core revision. This differs from Dynamic TX semantic revision, which advances only for semantic value/validity changes: a same-value sample may update sample time alone. A signal stale_ms of zero disables automatic timeout, with no global fallback. Source arbitration receives complete current/incoming values. See [dynamic TX](dynamic-periodic-tx.md).

## Protocol service boundary

### Current API

Adapters expose one on_frame/process/command/reset interface. meter_protocol_services_t carries Domain update, instantaneous Event and CAN TX with separate callback contexts. Application submits business commands through meter_runtime_command; Product routing selects the adapter. Temporary owner APIs and legacy dual callbacks are removed.

### Transactions

Signals are continuous state, Events instantaneous, Commands business requests, Transactions request lifecycles. Runtime has no singleton transaction or transport timeout. The old generic pool was removed after its only production consumer migrated.

### Boundary

Product workflows consume read/write results outside transport callbacks. Core knows no PDO; UI knows no CAN; Runtime has no product branches. Protocol calls are serialized and thread boundaries use native RT-Thread IPC.

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

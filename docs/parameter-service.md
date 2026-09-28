# Parameter transactions and permission grants

> [中文版](parameter-service.zh-CN.md)

This is an opt-in internal framework seam. No existing Product, UI, native worker, CAN encoder, SDO channel or NVM backend calls it yet. It closes the deterministic transaction boundary, not real-product parameter integration. Priorities and decisions remain in the [maintenance plan](maintenance-plan.md).

## Ownership and identity

The App owner exclusively calls `runtime/meter_parameters.h` and `runtime/meter_authorization.h`. There are no locks, callbacks, allocation, device calls or threads. The caller owns the service and immutable catalog for their complete lifetime. Do not copy, relocate or reinitialize a live service: its existing request ledger points to its embedded result slot. Catalog, service and call input/output storage must not overlap.

One service admits one transaction, retaining its terminal result until acknowledgement. This is a serialization/result-credit boundary, not a platform parameter-count limit. Catalog length is caller supplied; initialization validates every owner+ID pair. Independent backend channels may have independent services with distinct nonzero request sessions. A session must not be reused while old replies can survive; the integrator owns session allocation across resets. Serial and authorization epoch exhaustion fail closed rather than wrap.

`meter_parameter_key_t` names a logical Product endpoint and parameter. It does not encode bus, raw CAN identifier, SDO index/subindex or a storage offset. Equal numeric IDs under different owners are valid. Duplicate complete keys and zero owners are rejected. `meter_parameter_definition_t` is separate from the existing `meter_parameter_def_t` local settings catalog; neither MSP2 persistence nor its identifiers change.

## Catalog readiness and permissions

Zero-initialized descriptors are unconfirmed and cannot be read or written. A Product may mark an entry confirmed only after its mapping, representation, access and bounds have supporting evidence. Confirmed writable descriptors require a nonzero permission mask. Confirmed ranges must be finite and ordered. Unknown entries return NOT_FOUND; unconfirmed entries return UNCONFIRMED before reserving an identity or dispatching work. Generic fixtures contain only synthetic data. Confirmation is a Product assertion, not automatic evidence that hardware is ready.

Values are finite engineering-unit floats, matching the current Domain numeric convention. Exact wide integers, bitfields not representable in float, strings and opaque blocks are outside this seam. A backend must reject an unrepresentable conversion; it must not round a private wire encoding silently. Bounds do not imply a step size, enum membership, factory default, scaling or write/readback policy. Those require confirmed Product validation before submission.

A grant contains permission bits, a monotonically increasing epoch and expiry; credentials stay in Product code. Public reads can require zero permissions. Protected operations require every declared bit at admission, dispatch, timeout processing and reply handling. Regranting creates a new epoch, so revoke/regrant cannot revive old work. Invalid grants leave the prior grant intact; explicit revoke always clears permissions, including at epoch exhaustion. All calls for a transaction must use the same logical grant.

Time is monotonic milliseconds. Intervals are nonzero and less than the half range in `contracts/meter_time.h`. App must service deadlines within that comparison window and revoke expired grants before leaving it; this helper is not a wall-clock credential database. At exact expiry access is denied. A protected write that loses permission after dispatch may already have taken effect. This result is uncertain, not rolled back. Existing Demo authorization and health-mode policy are unchanged.

## Transaction lifecycle

| Phase | App operation | Result credit / effect |
|---|---|---|
| Free | `submit` validates key, confirmation, access, bounds and policy | Existing `meter_requests` ledger reserves identity and result slot |
| Queued | `take` rechecks permission/deadline, starts ledger and copies work | Dispatch boundary; backend may perform I/O after this point |
| Waiting | `reply` validates ID, owner+ID, operation and attempt | Wrong correlation is ignored; correlated malformed content is terminal INVALID_REPLY |
| Retry ready | `tick` expires one read attempt while total time remains | Next `take` increments attempt under the same request identity |
| Terminal | `query` copies result; `acknowledge` releases result credit | Repeated queries are stable; no new admission until acknowledgement |

Total timeout starts at admission, including time queued. Attempt timeout starts at `take`, including backend queue delay. Total timeout has priority over a retry and expires exactly at the deadline. Writes have exactly one attempt. Multiple read attempts require explicit `repeatable_read`: some reads have side effects. Automatic retry occurs only on attempt timeout, not explicit backend rejection/failure. A late reply cannot complete an expired attempt.

Results distinguish success, timeout, permission loss, caller cancellation, remote rejection, malformed correlated reply and transport failure. They carry the copied request/attempt, optional validated value and backend detail. Dispatched writes cancelled, timed out, malformed or transport-failed carry `effect_unknown`. REJECTED is reserved for a backend-confirmed refusal with no applied effect; if that cannot be established, report transport failure. OK means the backend-confirmed operation completed, not universal readback or durability. A durable local-setting adapter must wait for its storage acknowledgement before reporting OK.

The private ledger supplies reservation/retention only. Its generic cancellation code is not the parameter outcome; callers use `meter_parameters_query`, not the embedded ledger. Results do not change global mode or Domain faults. App decides how to present or classify them. `query` supports polling or copying a terminal result into existing owner IPC; no implicit callback thread is created.

## Backend integration obligations

The backend receives a copied `meter_parameter_work_t` and returns a copied reply through existing owner IPC. It must validate wire length, format, source, bus, key/operation mapping, representation and actual completion before constructing a reply. A local request ID cannot identify an untagged delayed wire response. The adapter must serialize and drain/quarantine ambiguous old responses before accepting a new attempt or request. Do not stamp the currently active ID onto any matching wire frame.

Cancellation and result acknowledgement do not cancel, drain or free backend I/O resources. Backend lifetime can exceed caller interest. `take` may be deferred while a channel is draining; total timeout still runs. Each adapter needs deterministic late-reply, cancellation and resource tests before production binding. Read retries also require this quarantine contract. No bus assignment, private checksum, CANopen object dictionary, retry timing or real-controller policy is inferred here.

Calibration should be an App workflow over fresh Domain measurements and typed parameter results. Prerequisite checks, source freshness, write uncertainty, optional readback and user notification remain explicit; do not place the transaction in an LVGL handler. Counter accumulation/migration, dynamic controller capabilities and shared identity providers remain separate open work in the active plan.

## Verification and limits

`tests/test_parameters.c` exercises duplicate owner-qualified keys, unconfirmed data, admission failure atomicity, permission masks/expiry/regrant, clock wrap, wrong replies, retained results, read retry, queued/active cancellation, write uncertainty and identity exhaustion. `tests/test_product_boundaries.c` characterizes the existing dual-bus router, timeout/value/source retention, recovery and independent 20/50 ms schedules. Synthetic deadlines are not physical CAN timing evidence.

Run these through CTest: `parameter-contracts`, `product-boundaries`, `guard-boundary-fixtures` and `assertion-witness`. Test assertions remain enabled in Debug and Release. The complete selected Product matrix and actual limitations are recorded in the maintenance plan. No current-tree firmware, controller interoperability, HIL, migration or calibration acceptance is claimed.

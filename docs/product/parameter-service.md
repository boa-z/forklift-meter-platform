# Parameter transactions and permission grants

> [中文版](parameter-service.zh-CN.md)

This opt-in framework service has a deterministic reference Application and copied presentation witness. Demo binds its own synthetic catalog in `products/demo/services/settings_app.c`; real controller transports and persistent parameter backends are not automatically bound. The active priorities remain in the [maintenance plan](../maintenance/maintenance-plan.md).

## Product definitions and independent backends

Framework provides types, catalog validation, authorization checks, request identity, deadlines and retained results. It defines no vehicle parameter IDs, defaults, ranges, permission meanings or fallback catalog. Every production parameter definition belongs to the selected Product package, including local Settings descriptors. Product owns semantic-to-key mappings, units, supported operations, credentials, protocol encoding and optional persistence binding. Tests may construct synthetic catalogs; they are never firmware defaults.

The public `meter_parameter_exchange_t` in `contracts/meter_parameter.h` describes an optional App-level copied-message port: context, ready, send and receive. It has no CAN, device, RTOS, storage handle or record-layout dependency. The transaction engine neither stores nor calls it: App orchestrates the exchange using the existing take/reply API. Direct users of take/reply remain valid. The reference name `reference_parameter_port_t` is a source-compatible alias.

All callbacks must be valid and nonblocking for the duration of each App call. Successful send copies the work before returning; it never borrows App stack storage. Receive copies and consumes one completed reply. Ready includes backend drain/quarantine and capacity, separately from retained-result credit. A send failure after take keeps the existing conservative failed-write semantics. Transport and storage owners perform their own work outside App callbacks. A storage adapter promising durability reports OK only after durable completion, not queue admission; failure uses the existing backend failure outcome and detail without changing health policy. No serializer, storage format or additional worker is introduced.

Local authoritative Settings (`meter_parameter_def_t`, Domain actions/snapshot and the existing NVM path) and owner-qualified Parameters (`meter_parameter_definition_t`, explicit request/result lifecycle) remain separate. The exchange can bind a Product-selected backend; it does not automatically route local Settings through transactions or migrate their records. Exactly one build-time Product owns each firmware catalog.

The architecture guard rejects direct parameter descriptor arrays and initialized descriptors in contracts/core/runtime, while permitting borrowed catalogs and Product-owned definitions. Negative fixtures cover both descriptor types, all three Framework layers and forbidden runtime-to-storage imports. This is a lexical regression guard, not proof against aliases, macros or all semantic dependencies.

`tests/test_parameter_app.c` uses the public exchange for the existing synthetic protocol backend and a deferred storage-owner fixture. It checks that admission/dispatch do not imply durability, reads return completed storage values, failures retain diagnostic details and uncertain write effects, and independent supplied catalogs retain their own access/range rules. These are deterministic integration witnesses, not physical transport or power-loss evidence.

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

Calibration should be an App workflow over fresh Domain measurements and typed parameter results. Prerequisite checks, source freshness, write uncertainty, optional readback and user notification remain explicit; do not place the transaction in an LVGL handler. The existing calibration/profile contracts are documented in [Application services](application-services.md); counter semantics and shared identity providers remain open decisions.

## Verification and limits

`tests/test_parameters.c` exercises duplicate owner-qualified keys, unconfirmed data, admission failure atomicity, permission masks/expiry/regrant, clock wrap, wrong replies, retained results, read retry, queued/active cancellation, write uncertainty and identity exhaustion. `tests/test_product_boundaries.c` characterizes the existing dual-bus router, timeout/value/source retention, recovery and independent 20/50 ms schedules. Synthetic deadlines are not physical CAN timing evidence.

Run these through CTest: `parameter-contracts`, `product-boundaries`, `guard-boundary-fixtures` and `assertion-witness`. Test assertions remain enabled in Debug and Release. The complete selected Product matrix and actual limitations are recorded in the maintenance plan. No current-tree firmware, controller interoperability, HIL, migration or calibration acceptance is claimed.

## Reference Application and presentation boundary

`examples/parameter-workflow/app.c` is an opt-in reference App owner, linked only by the CTest witness. It composes the existing transaction service and grant with three nonblocking copied-message port operations: ready, send and receive. A real Product implements these through its protocol owner's IPC; the reference does not select CAN, encode frames, load credentials or change the production App callback ABI. It is not an additional Product linked into firmware.

App consumes a value intent containing a profile generation, panel lifetime token, Product field, operation and desired value. Stale profile generations and zero panel tokens fail before admission. The App owns timeout/retry policy; UI cannot change it. Authentication code running under App ownership supplies/revokes the grant. Every App step processes deadlines, consumes at most one reply, dispatches at most one attempt when the backend is ready, and copies a terminal result into its presentation value. Call steps and intent/ack handlers serially; initialized objects, non-overlapping output storage and valid nonblocking port callbacks are required.

`ui/presentation.h` contains only value contracts. Pending never means success and contains no optimistic confirmed value. UI renders a result only when its profile generation and panel token match and `has_result` is true. A cancelled/timed-out dispatched write may have taken effect; preserve `effect_unknown` instead of showing rollback. The view copies only outcome, availability/value, attempt count and effect_unknown; remote keys, work/reply structures and backend detail remain in App diagnostics. These values do not change machine health. A terminal result remains retained until App receives acknowledgement of both its panel token and request identity. A replaced panel may ignore an old result, but App still must explicitly acknowledge it; ignoring a publication never frees result credit. Product policy must decide who acknowledges orphaned panels.

Backend readiness includes resource ownership and drain/quarantine independently of UI result credit. Cancelling or acknowledging a write leaves the backend busy. A later intent may be admitted but cannot dispatch until that backend releases its channel; the total deadline still runs. The test backend returns the original copied token on late completion, never stamps a newer request onto it. If a ready/send race rejects a handoff after `take`, the reference retains TRANSPORT_FAILED with conservative write uncertainty and never resubmits the write. Real adapters still need wire-level correlation/drain evidence.

`tests/test_parameter_app.c` verifies admission and owner-specific ranges, stale profile/panel filtering, copied input/output lifetimes, retained results, permission expiry before dispatch and revoke/regrant after dispatch, cancellation with live backend ownership, late completion during a new request, queue rejection, read retry after drain, malformed replies and queued total timeout. All inputs and replies are synthetic. Existing `parameter-contracts` still covers the wider identity, clock-wrap and outcome matrix. Run `ctest --test-dir BUILD --output-on-failure -R parameter` in Debug and Release.

Production integration decision: bind Product-specific descriptors, authentication and semantic IPC endpoints only after backend mappings and lifetime/correlation rules are confirmed. Keep exactly one Product per image. Catalog/profile replacement must explicitly address session allocation, in-flight cancellation, resource drain and retained results before reinitialization; the reference deliberately exposes no live reset shortcut. Local Settings/MSP2 and static auth booleans remain separate, as documented in D-06 in the maintenance plan.

## Product field mapping

The reference UI names TRAVEL_LIMIT, LIFT_LIMIT or AUXILIARY_LIMIT. Only app.c maps these synthetic fields to owner-qualified catalog keys; two owners deliberately reuse object 7. Unknown fields fail before admission, unconfirmed catalog entries remain unavailable, and owner-specific permissions/ranges still apply. This is one Product example, not a configurable universal parameter-panel schema. Production code replaces the local mapping and descriptors after confirmation.

Reference-example compatibility: callers of examples/parameter-workflow replace intent.key with intent.field and consume flat ViewModel fields instead of result internals. These symbols are CTest-only, outside firmware source manifests. Runtime parameter contracts, wire behavior and storage are unchanged. Opaque request IDs remain only for exact acknowledgement/cancellation; UI must not decode them.

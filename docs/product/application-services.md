# Application services and Product presentation

## Ownership and compatibility

Domain / Runtime state flows through Product Application services and typed Product presentation values into LVGL. Demo builds products/demo/application/presentation.c without LVGL, projecting the existing consistent snapshot into owned scalars, validity states and immutable catalog strings. Screens render these values and emit intents; local Setting intentions remain distinct from confirmed state. No reactive engine, second snapshot transport, new worker or lock is added. Unknown/stale/error never becomes valid zero. Existing units, thresholds and local Settings policy remain; an unavailable local setting no longer silently overwrites the widget with zero.

The optional Product manifest application group is selected by the same single METER_PRODUCT_ROOT in CMake and firmware. Each firmware still contains exactly one Product. Reference parameter examples and synthetic calibration fixtures are host evidence, not additional firmware Products or production transport bindings.

meter_snapshot_t gains an in-memory profile field. Rebuild all consumers together; this is not a stable binary ABI for precompiled clients. MSP2 serialization, CAN encoding, worker ownership, scheduling, update trust and native failure behavior are unchanged. Demo leaves dynamic profile unknown until a Product explicitly supplies a mapping. This does not hide existing static Demo screens.

## Profile publication and consumers

App alone maps Product raw identity/features into meter_profile_t: confirmed stable family, normalized capabilities and generation. Family zero/unconfirmed means no usable dynamic profile. meter_core_profile rejects malformed state and generation exhaustion, leaves identical refreshes unchanged, and increments generation/revision for semantic changes. Generation never wraps; session restart is a new lifetime, not permission to reuse old panel/backend tokens. Profile travels with existing locked deep snapshot publication. The setter itself is serialized App code, not an atomic cross-thread API.

Before publishing a changed profile, Product App must invalidate dependent measurements, fault catalogs, identity/version caches and presentation selection in the same App turn. Do not relabel old measurements with the new generation. Core cannot infer Product dependencies and deliberately does not clear unrelated signals. A synthetic Product mapping fixture proves raw feature positions end in adaptation; generic code sees normalized values only.

| Consumer | Required handling |
|---|---|
| Presentation / feature visibility | Compare generation, rebuild from current copied snapshot; unconfirmed dynamic capabilities are unavailable |
| Parameter panel | Match panel lifetime token and profile generation; retain old result under its original identity |
| Calibration | Attest measurement acquisition generation; reject stale/mismatched capture; invalidate active run on profile change |
| Fault catalog / version cache | Mark old interpretation unavailable before next publication; Product chooses catalog and reacquisition policy |
| Backend | Keep independent outstanding request/quarantine credit; UI close/profile replacement is not wire cancellation |

The reference parameter App explicitly cancels caller interest and revokes its grant on replacement. This is synthetic example policy, not a production safety decision. Successful or uncertain old results remain historical outcomes, never current-profile success. D-07 still requires Product review of cache invalidation, authentication and wire-response correlation.

## Capture and remote calibration

runtime/meter_calibration is an opt-in App-owned service with one capture/write/optional-readback run. Product provides source and owner-qualified target in the same engineering units, age bound, prerequisites, authorization and transaction policy. It has no transport, UI, persistent state or credentials. Initialize once; serialize calls in App; never copy live service state. contracts/meter_calibration.h contains copied presentation data only.

Begin captures one finite VALID value, checks age with half-range time rules, confirmed profile and measurement-generation attestation, then admits a write. Step/take recheck generation, prerequisites and authorization; queued captures expire before dispatch. After dispatch the captured value intentionally stays stable. Take requires separate backend readiness/drain credit. Replies carry existing exact transaction identity; late/foreign replies cannot finish a replacement run. No write retries are introduced.

DONE retains separate typed write/readback results until exact generation/token acknowledgement. Dispatched-write effect_unknown survives cancellation, profile loss, expired permission, transport failure and timeout: the write may have applied; this is not rollback. Successful write plus failed/mismatched readback remains observable. Check phase before terminal outcome. Old DONE values stay immutable; consumers must reject stale generation. Grant expiry before dispatch produces no uncertain write.

Synthetic tests cover invalid/stale/unknown/future measurements, clock wrap, queued expiry, malformed configuration, generation replacement, prerequisites, actual permission expiry, rejection, timeout, transport failure, optional/no readback, mismatch, late replies and panel acknowledgement. They do not prove customer mappings, actual sensor freshness, ECU behavior or physical calibration accuracy.

## D-01 classification decision

meter_parameter_classify is a pure typed mapper. Expected suppression, communication stale, remote rejection, timeout, transport failure, resource pressure, persistence failure and contract violation are distinct vocabulary. Non-parameter event sources are not automatically wired into policy. No classifier mutates global mode; semantic rejection does not automatically become DEGRADED.

| Class | Decision before health integration |
|---|---|
| Expected suppression / remote rejection | Usually operation-local; confirm Product policy and diagnostic visibility |
| Communication stale / timeout / transport failure | Define required source, duration, recovery and safety relevance |
| Resource pressure / persistence failure | Define bounded recovery, durability requirements and escalation threshold |
| Contract violation | Define fail-safe behavior, reset authority and retained evidence |

D-01 remains PENDING human/Product review: approve event-to-mode mapping, hysteresis and recovery ownership before connecting classification to NORMAL/DEGRADED/FAULT. Existing health behavior stays intact. This is not a DTC subsystem.

## D-05 counter decision

No persistent counter service is implemented. The following proposal bounds the decision; none of its Product policy values is approved.

| Topic | Proposed contract and required decision |
|---|---|
| Accumulation unit | Unsigned 64-bit milliseconds for hours; integer distance plus fractional remainder for odometer; confirm distance quantum and sensor accuracy |
| Activation | Product supplies explicit key/seat/work predicates and fresh valid speed; decide missing/stale source behavior per counter |
| Overflow | Saturate with visible terminal diagnostic, never wrap; approve lifetime/range and recovery |
| Reset | No implicit reset of total odometer/hours; separate trip/service counters require explicit authorization and audit policy |
| Discontinuity / power loss | Never integrate unbounded gaps; define maximum interval and permissible loss since durable checkpoint |
| Checkpoint acknowledgement | Track requested, RAM and durable revisions separately; submission is not persistence success |
| Representation / versioning | Explicit-width versioned records with migration/default policy; no native struct dumps or change to current MSP2 in this batch |
| Wear budget | Derive checkpoint policy from endurance, bytes per write, lifetime and acceptable loss; no universal Flash interval |

D-05 remains PENDING approval of semantics and actual device endurance evidence. App consumes a storage service contract, not NOR/NAND/EEPROM geometry. D-03 partial initialization, one-shot shutdown and unbounded durability/acknowledgement waits remain characterized and unchanged.

## Identity provider assessment

Immutable firmware/build metadata and dynamic controller identity have different lifetimes. A future small read-only Product identity view should carry typed source, availability, generation and bounded text/value storage; diagnostics/UI/maintenance can copy it. Update retains its trusted compatibility checks. Do not unify SDO, CAN or firmware metadata wire representations. No shared provider is added yet: confirm first Product consumers and profile invalidation policy before adding another public contract. D-07 records this remaining integration decision.

First-party explanatory code comments use Chinese. API identifiers, units and tool directives retain their original spelling; third-party notices remain intact. Documents continue to use the existing English/Chinese pairs. The adaptation follow-up translates the new presentation/profile/calibration/classification contract comments without changing executable code.

## Executable Product adaptation recipe

The CTest target product-application-flow builds tests/test_product_application.c only under BUILD_TESTING. It composes the existing Core, authorization, calibration and classification APIs; it adds no production service, worker, transport or Product registration. The actual firmware still selects exactly one Product at build time. Together with the Demo headless projection and LVGL renderer, this is a concrete reference for adaptation.

The fixture keeps its catalog, raw identity/features mapping, stationary prerequisite and cache/grant invalidation policy inside Product App functions. On a semantic profile change it invalidates the dependent measurement, clears the acquisition generation, revokes the reference grant and processes the active calibration before making the next copied presentation. Identical normalized features leave generation/revision unchanged. The test is serial; real integration uses the existing owner and locked snapshot publication boundary, not these test-local calls as a new synchronization mechanism.

The Product ViewModel copies revision/generation, measurement with validity, a semantic capture-supported flag, pending/result availability and the original result generation/token/outcome/uncertainty. UI does not decode family, feature bits or remote parameter addresses. A historical uncertain write remains visible to App policy but cannot appear as current-panel success. Result fields are meaningful only when has_result is true; no zero-initialized outcome is an optimistic success. Calibration results may change without a Domain revision change: App must publish those changes through its existing copied presentation channel and must not use Domain revision alone to suppress them.

Deterministic cases cover unknown/unsupported capabilities, capability changes, ignored raw bits, valid zero versus stale/unavailable readings, rejection of old-generation measurements, profile replacement during a dispatched write, exact result acknowledgement, independent backend drain, late replies during a new operation, stable captured values, successful readback and authorization expiry classified as expected suppression. Classification does not mutate health. Backend completion in this fixture is synthetic evidence, not real wire correlation or physical timing acceptance.

For a real Product, replace the local mappings/catalogs, capability normalization, acquisition identity attestation, prerequisites, ViewModel and UI composition. D-07 must confirm those policies, authentication and actual backend correlation/drain; D-01 still owns health consequences. No customer IDs, credentials, counter persistence, production safety rules or universal page/workflow schema are chosen by this example. Run product-application-flow together with parameter-app-boundary, product-presentation, runtime-profile and calibration-workflow in Debug and Release.

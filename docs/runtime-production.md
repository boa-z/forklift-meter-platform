# Production runtime

## Current architecture

Firmware uses separate Protocol, App/Core, CAN TX, LVGL UI, NVM and Update owners with bounded native RT-Thread IPC. The old single-thread CAN/UI smoke and separate OTA RX/TX path have been replaced by the shared execution port. CAN draining does not run in the UI chain, and publication locks do not cover driver or LVGL execution.

## Boundaries

Core and common runtime remain OS/LVGL independent. Protocol owns decoding and protocol state; App alone owns Core and product workflow. Only TX workers may call the CAN driver write API. UI receives copied arrays and submits copied intentions. Native bounded queues reject whole messages on overload; no generic event bus is introduced. Initialization opens CAN only after IPC and workers are ready. Stop must wait for in-flight I/O; a timeout reports incomplete stopping and never destroys a live worker.

## Acceptance status

Current Host, Linux analysis and single-bus hardware evidence is recorded in [validation](validation.md). Records retain original firmware SHA and image hashes; history consolidation does not change the installed firmware identity. Dual-bus hardware remains unverified; no formal MISRA, rollback or power-loss recovery claim is made.

## Execution and ownership

| Owner | Trigger / blocking boundary | Static stack |
|---|---|---|
| Protocol priority 19 | RX semaphore, maximum 64 frames/pass, round-robin buses; 1 ms UDS or 5 ms ordinary wait; no driver write | 8192 bytes |
| App priority 20 | Batch/event/action wake or 5 ms tick; no device/Flash/LVGL | 8192 bytes |
| CAN TX priority 18 | One writer per bus, queue wake, idle blocks forever; blocking driver write | 2048 bytes/bus |
| UI configured priority + 2 | LVGL 16 ms normal / 50 ms maintenance | Default 32768 bytes |
| NVM priority 21 | Existing queue; blocking EEPROM I/O | Existing NVM budget |
| Update priority 25 | Existing queue; Flash and durable barrier wait | 12288 bytes |

Core and Product workflow run only in App. Reference-Mixed startup synchronization uses semantic commands and decoded parameter events, never a CANopenNode channel. The immutable Product getter no longer resets protocol state. Host tests drive App and Protocol steps separately; they do not simulate native scheduling.

## Ports and lifetimes

Protocol stages at most 32 values per decode, then copies the whole semantic batch into one of 64 native queue slots. App validates every value and source arbitration before any write. Queue-full and oversized batches are rejected in full. Events use 16 copied slots. Old-generation batches/events are discarded. Product callbacks borrow values only during the call.

UI has one request credit and one retained result. Successful submission means QUEUED; App reports APPLIED or REJECTED. Core, UI publication, diagnostic publication and UI consumer storage are distinct, with alias rejection. UI never stores Core arrays.

App has one retained command/result credit, identified by generation and a non-reused 64-bit serial. QUEUED, APPLIED, TX_COMPLETED and REMOTE_CONFIRMED differ. Synchronous command frames are tagged and report TX_COMPLETED after driver completion. Asynchronous SDO reports REMOTE_CONFIRMED from its response; uncorrelated driver completion is not fabricated. Timeout/cancellation stops local retries through adapter cancel/reset. Already delivered remote writes may have taken effect; local cancellation does not promise rollback. Product must reconcile uncertain remote state before retrying non-idempotent commands.

Each bus has 16 ordinary and 16 urgent TX slots; at most four urgent frames precede an ordinary service opportunity. Old-generation frames are discarded. Queue acceptance does not prove physical delivery. No generic event bus or cross-owner temporary pointer is used.

## Modes and deadlines

App owns STARTUP, NORMAL, DEGRADED, UPDATE_MAINTENANCE and SHUTDOWN. Product supplies the capability matrix. An increase in the native queue-full counters (`batch_full`, `event_full`, `tx_full`) selects DEGRADED when maintenance is not selected; one second without another queue rejection permits recovery. Semantic batch validation and TX publication failures increment `batch_rejected`, which is not part of that overload calculation. Changing this boundary requires the product-policy review recorded as D-01 in the [maintainer assessment](maintainability.md). This is a reference policy, not a safety guarantee. Mode changes advance generation, stale old Domain signals and reset Protocol sessions and Product workflow. Returning from OTA requires ending the explicit maintenance request; Abort does not override a technician's maintenance selection.

Reference-Demo sends synthetic CAN0 0x3C0 every 50 ms and 0x2F0 every 100 ms. Only 0x3C0 is critical in maintenance. These public bench frames are not vehicle control. Deadlines advance from their previous planned value. Late runnables skip missed periods without catch-up bursts. Mode changes explicitly rearm phase. Mixed TPDO uses the same helper. Wire spacing still depends on queue and driver latency and requires PCAN measurement.

Dynamic values, sample/publish time, semantic revision, generation, pending replacement and expiry are specified in the [Dynamic TX contract](dynamic-periodic-tx.md). Periodic messages use separate bounded mailboxes and do not accumulate historical values in ordinary TX queues.

## Lifecycle and diagnostics

INIT prepares static IPC and snapshots. App starts NVM/Update and TX before Protocol opens CAN. READY becomes RUNNING only after all configured buses open. Settings wait for NVM restore. Failures observed by the running App enter FAILED then cooperative STOPPING. Pre-App partial initialization instead returns failure while retaining earlier native objects; App-thread startup failure leaves initialized/FAILED without an App to coordinate stop. Neither path has a proven rollback/retry contract; see D-03 in the maintenance plan.

The meter_exec stop command rejects new business/TX, cancels queued work, stops Protocol processing, requests Update stop, and flushes NVM to a durable revision. In-flight blocking I/O returns normally. App waits for all owner acknowledgements before unbinding RX and closing CAN; UI releases LVGL last. After five seconds STOPPING reports overdue, without killing or freeing workers. STOPPED requires reboot to restart; static IPC is retained until reboot. A failed durability barrier deliberately prevents a successful STOPPED claim.

Owners copy diagnostics under short locks; MSH formats outside locks. No device/Flash/LVGL work holds the publication lock. Trace publication copies at most 16 entries per lock. meter can includes native FIFO drops. meter_exec exposes mode/generation, queue high-water/rejections, runnable counts, planned misses and stop wait. Native list_thread supplies stack high-water. Idle blocking workers are healthy waiting states; runnable counts are observations, not watchdog verdicts.

## Quality and gates

Portable runtime enables strict warnings incrementally. Linux CI configures cppcheck, clang-tidy analyzer/bugprone/cert/performance, ASan/UBSan and bounded libFuzzer coverage for records, settings, package parser, request ledger and protocol decode. Adopted third-party sources are unchanged. The ownership source gate is a regression guard, not a proof of freedom from races.

Windows Host, target build and physical HIL are reported separately with SHA-bound evidence. Configuring Linux analysis does not prove it ran. Multi-bus HIL needs two physical interfaces and matching Product firmware. Target warning coverage, scheduler stress, long-duration timing and shutdown during each backend operation remain production gates until measured. No formal MISRA, AUTOSAR conformance, rollback or power-loss recovery claim is made.

Product may declare command completion at APPLIED, TX_COMPLETED or REMOTE_CONFIRMED; the default requires the remote response. A transmit-only command retains TX_COMPLETED as its final result and does not later become a false timeout. Session adoption resets adapters and publishes the same generation as App even while telemetry is suppressed.

Actual analyzer coverage, pending human approval of Annex K TAD-001 and required-check gaps are maintained in [governance status](compliance/status.md). The advisory exception is not an approved MISRA deviation.

## Build-time Product selection

Each firmware image contains exactly one Product, selected by the singular `METER_PRODUCT_ROOT` environment variable in SCons. The default is `products/demo`; relative selections resolve from the application root, and external package roots are supported. Use separate build/output directories for different Products. Host CMake likewise selects one Product per configuration. Multiple independently tested Products never imply multiple Products in one firmware or runtime switching.

The selected `product/sources.json` supplies the unique firmware composition implementation. It owns independent static Domain/publication/diagnostic/UI storage and the Product locale setup callback through `contracts/meter_firmware.h`. Generic startup knows only that contract. Demo retains its original store capacities and locale initialization order; Reference-B has separate signal-only storage. Inspect the selected source closure with `tools/firmware_product.py`; unsupported feature closures and missing/escaping/duplicate sources fail the build selection.

Demo and Reference-B have independent host compile/link/test evidence for this boundary. Reference-Mixed's SDO firmware closure is not enabled. Any real Product still requires target memory/display/CAN adaptation, confirmed protocol/authentication descriptors and board acceptance. The reference parameter Application in `examples/parameter-workflow` is test-only; it adds no Product to an image and changes no native IPC, worker or timing contract.

Demo PDO addition: four periodic slots and ten semantic values fit the existing board budgets of eight slots and sixteen values. No queue capacity, owner, worker or scheduler period changes. The two new ordinary 100 ms frames add 20 frames/second; bus loading and existing 50/100 ms traffic must be measured together. See [protocol definitions](protocols.md) for payload/freshness and the shared-revision implication.

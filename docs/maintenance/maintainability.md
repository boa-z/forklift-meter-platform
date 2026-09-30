# Maintainer assessment

> [中文版](maintainability.zh-CN.md)

Inspected 2026-09-28 at application commit `81e508314c5c36ea2e01dc797a8caf29a67f8470`, initially clean on `codex/dynamic-periodic-tx`. This is a source reconstruction, not an inherited acceptance verdict. Findings below refer to that baseline; completion is tracked in the [execution plan](maintenance-plan.md).

## Assessment

The platform has a coherent portable Domain and explicit production owners, with unusually substantial negative-path tests for a reference application. Preserve those boundaries. The principal maintenance risk is concentrated native orchestration and dense handwritten state-machine code, rather than a missing framework. Product independence is demonstrated on the host, but firmware composition and hardware qualification lag behind that claim. It is a useful engineering reference, not yet evidence of a qualified vehicle product.

Inspection covered build manifests, CI, contracts, portable runtime/Core, native owner startup/loop/stop paths, storage handoffs, protocol adapters, update integration, tests and current guides. This is a focused first audit, not an exhaustive line-by-line safety or race analysis. Historical memory supplied no application implementation facts.

## Reconstructed architecture

| Boundary | Actual implementation and flow | Maintenance implication |
|---|---|---|
| Product composition | `CMakeLists.txt` selects `product/sources.json`; Demo and two examples supply catalogs, routes, capabilities, policies and UI. `SConscript` instead selects Demo explicitly; `main.c` includes Demo storage and i18n. | External Product host builds do not establish replaceable firmware composition. |
| Portable Domain | `contracts/` describes IDs, values, batches and ports; `core/meter_core.c`, settings and snapshot helpers validate and store caller-owned arrays. | Product identities are handles, not array indices. Core needs no OS or LVGL. |
| Protocol | `runtime/meter_runtime.c` routes frames to adapters, stages complete semantic batches and reports identified command progress. `protocols/common/` contains routing; generated DBC adapters live with Products. | Production Protocol is the sole protocol-state owner. App consumes copied semantics. |
| Native execution | `platform/rtthread/meter_execution_port.c` contains IPC, App/Protocol/TX entrypoints, mode/generation changes, publication and shutdown. | This is the main concurrency composition root; portable runtime is not the complete production scheduler. |
| Periodic transmission | App samples and deep-copies publication; Protocol owns deadlines, encoder and wire state; per-entry native mailboxes hand frames to bus TX workers and return identified results. | Sampling, semantic revision, publication, admission and completion must remain separate. No historical backlog. |
| Commands and services | Product App workflow submits semantic commands; Protocol routes/encodes; bus TX reports driver completion. Mixed SDO uses pinned CANopenNode and remote responses. | APPLIED, TX_COMPLETED and REMOTE_CONFIRMED are different evidence. Local cancellation cannot undo remote writes. |
| UI and diagnostics | `main.c` owns LVGL, reads copied presentation arrays and submits intentions; separate diagnostic arrays and short locks support MSH formatting. | UI/diagnostics must never retain mutable Core arrays. Initialization constructs Core before handing mutation to App. |
| Persistence | `storage/` provides records, two-slot recovery and NVM state; `meter_nvm_port.c` transfers work/results to a blocking worker. | RAM application, queued persistence and durable revision are distinct. Storage format is a compatibility contract. |
| Update | UDS/ISO-TP on the common Protocol/TX path admits copied jobs; Update worker validates packages and uses the native SDK installer with NVM barrier. | Hash integrity is not authenticity; native boot confirmation is not application-health confirmation. |

Start a code review with [runtime ownership](../runtime/runtime-production.md), [dynamic TX](../runtime/dynamic-periodic-tx.md), [protocols](../runtime/protocols.md), [NVM](../runtime/nvm.md) and [update](../ota/can-update.md). Source manifests select dependencies; upstream code remains in pinned submodules.

## Findings and priorities

Priorities describe engineering sequence, not a vehicle safety classification. Open decisions do not authorize semantic changes.

| ID / priority | Evidence at baseline | Consequence and next action |
|---|---|---|
| M-01 / high | `meter_execution_port.c` is 880 lines: several owners, shared flags, five queue families, periodic mailboxes, command ledger, publication and lifecycle in one translation unit. `command_poll`, `periodic_poll`, `app_entry` and `copy_traces` compress multiple transitions into individual lines. | Add a state/lock map and lifecycle tests before considering file extraction. Preserve locking, priorities and call order. A file split alone is not an ownership proof. |
| M-02 / high | `runtime/meter_periodic.c` uses short parameter names and multi-statement branches across validation, deadline consumption, freshness and commit policy; its existing test is one long scenario. | First bounded hardening target: explicit names/branches and named characterization tests without altering arithmetic or policy. |
| M-03 / high | `app_entry` detects overload from `batch_full + event_full + tx_full`; invalid semantic batches increment `batch_rejected` separately. The runtime guide says batch/event/TX rejection enters DEGRADED. | Correct the guide to the actual counter boundary. Whether validation rejection must change mode is decision D-01, not a cleanup. |
| M-04 / high | Host supports `METER_PRODUCT_ROOT`; firmware `SConscript` and `main.c` name Demo directly. Runtime accepts Product hooks, but board entrypoint does not yet expose equivalent selection. | Decision D-02 must specify firmware Product selection, storage binding, UI bootstrap and target source closure before generalizing it. |
| M-05 / high | `tests/test_execution_port.c` includes the native `.c` file and models queues deterministically; thread init/start, delay and hardware stubs assert if reached. `test_publication_threads.c` exercises publication under host locking. | Useful unit evidence, but native startup, partial failure, stop and priority scheduling are not exercised by these tests. Add deterministic lifecycle scenarios; reserve real scheduling and blocked I/O claims for target evidence. |
| M-06 / high | CI portable analyzers cover core/runtime/storage/update; generated analysis adds seven translation units. Handwritten Product, platform, UI and target-specific configurations are incomplete, as governance already acknowledges. | Inventory first-party translation units against actual compile commands; expand analysis incrementally without new blanket suppressions. |
| M-07 / medium | `check_architecture.py` resolves includes from repository root, not the including file. `check_runtime_ownership.py` selects Protocol by splitting between function names and checks tokens, not callees. No dedicated negative-fixture tests for these guards are registered. | Relative includes, helpers and refactoring can evade or break guards. Add failing synthetic fixtures before strengthening resolution/scope. Keep source checks described as regression guards. |
| M-08 / medium | Many C tests put both setup calls and checks inside `assert`; CI explicitly uses Debug, while CMake permits Release with NDEBUG. | Do not count Release CTest green as equivalent until test assertions are mechanically guaranteed active. Reproduce with a deliberate failing fixture and add a test-only policy; do not alter production assertion semantics. |
| M-09 / medium | Bilingual gate compares heading/block/table/link structure but does not verify link existence or translation meaning. Several callback comments still say Protocol/App despite production single-owner rules. | Correct verified ownership wording and add link/negative-fixture coverage later. Bilingual structural green is not technical review. |
| M-10 / high | `docs/compliance/status.md` records pending TAD-001 and absent protected checks at its inspection; workflow YAML cannot enforce branch protection. | Preserve pending status. Administrator approval and live repository enforcement verification are needed; this audit did not query remote settings. |
| M-11 / medium | `docs/build/migration.md` still labels CANopen as staged and firmware update outside scope; its protocol/UI paths and the Demo storage path in `docs/product/downstream.md` no longer match the tree. | Repair the map and distinguish original extraction history from present capabilities. Preserve historical validation identities. |

## Product readiness decisions

| Decision | Required human/integration decision | Evidence needed before closure |
|---|---|---|
| D-01: rejection policy | Keep DEGRADED limited to IPC overload, or include invalid semantic batches, failed sample publication and other rejects? Define recovery and diagnostic expectations. | Product policy, counter-specific tests and target fault-injection traces. Current behavior remains unchanged. |
| D-02: firmware composition | Choose the Product selection and board-binding contract; retain a narrow composition root instead of adding a generic registry. | Two independently selected firmware source closures, preserved Demo build, correct LVGL linkage and matching board validation. |
| D-03: lifecycle failure policy | Define partial initialization cleanup, restart prohibition and failed durability-barrier handling before modifying native startup/stop. | Inject every IPC/worker/open failure; prove no live resource is freed and no incomplete stop reports success. Include stop during each blocking backend operation. |
| D-04: real product qualification | Define actual protocol authority, diagnostics/safety policy, timing limits, bus-off behavior, NVM endurance and update authenticity/recovery requirements. | Approved requirements plus source/image-bound physical tests. Synthetic Demo traffic and existing single-bus measurements are not vehicle qualification. |

The native update limitations in `docs/ota/can-update.md` are explicit and significant: vendor early auto-confirmation, no claimed authenticated update or rollback, and unverified power interruption during download. Do not quietly change boot trust or recovery under a maintainability task.

## Verification and confidence

Fresh Windows GNU 16.1.0 / Python 3.13.15 Debug headless build with optional update enabled passed **42/42 CTests** before edits in `build-maintainer-audit`. This rebuild used current sources, not an old executable. It covers portable behavior, deterministic native-port paths, public headers, generation and repository guards. Post-change results and exact commands belong in the [execution plan](maintenance-plan.md).

The source-bound [validation record](../testing/validation.md) remains historical evidence for its named commits and images. This audit does not transfer that hardware verdict to the current tree. Linux sanitizers/analyzers, fresh firmware build, physical UI, dual-bus hardware, prolonged timing and power-loss tests are not established by this host run. The enclosing SDK and other application checkout had existing state; no configuration, gitlink or sibling-source change is part of this work.

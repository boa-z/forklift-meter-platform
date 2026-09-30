# Active maintenance plan

> [中文版](maintenance-plan.zh-CN.md)

Established 2026-09-28 from the [source assessment](maintainability.md), baseline `81e5083`. This is the active continuation record. Update status and evidence here in the same change as the work; do not depend on chat history.

## Documentation organization and OTA workflow (2026-09-30)

The maintainer requested module folders and an operator-first CAN OTA guide. Documentation now uses build, product, runtime, ota, testing, maintenance and compliance modules, each with a bilingual index. Root navigation starts with tasks. Relative links and first-party references follow the moved files; historical source/image identities stay unchanged.

CAN OTA now leads with firmware artifacts, packaging and installation commands, followed by explicit candidate/activation/reboot checks, failure actions and terms. SDK review and technical investigation are kept in a separate reference. Command fields were checked against the current CLI and board diagnostics. Documentation checks do not establish a new hardware or power-loss result.

Validation: 37 bilingual pairs and the unchanged public-clean gate passed; seven documented OTA CLI invocations parsed without network I/O. The existing Demo OS was packaged using the SDK cpio/mkenvimage executables and passed real offline preflight with the shared inspector. Package size: 1,115,136 bytes; SHA256: 06b42453fb10c31efc9d1150ca07894c419ec4fda9281dcf0d519eb2476ef233. No firmware was transferred or activated during this documentation task.

## SDK and host build compatibility (2026-09-30)

Actions run 36659079158 exposed Python 3 text decoding and Product path-type regressions after the SDK Python 2 compatibility change. Preserve Python 3 Path return values, Python 2 string paths and JSON CLI serialization; decode only bytes when emitting identity headers. The optional firmware OTA path now uses a Python 2-compatible exact version match and Unicode text output. Public build examples use a generic board alias, correct Product path resolution and explicit SDK environment prerequisites; no cleanliness rule or CI check is weakened.

Validation of this working-tree patch: Python 3 helper regressions passed 6/6; the full host pytest suite passed 119 with 14 environment-dependent skips; Debug/headless+update CTest passed 88/88. Both helper CLIs ran under the unchanged SDK Python 2.7. An OTA-enabled Demo build using that interpreter and the SDK shell environment produced the image, ELF, map and OS; all selected Product sources were verified in the link map, 795 compiler commands were captured and the saved SDK configuration/header bytes were restored. The local build report records the dirty source identity; it is build evidence, not board acceptance. No hardware flashing or OTA transfer was performed for this repair. Remote Actions results must be checked for the pushed revision.

## Optional-storage update barrier (2026-09-29)

Framework `9b9e671` fixes the activation barrier for Products without enabled settings storage. Previously no nonzero revision could be produced, so activation would time out. The App now completes that barrier only when storage is absent or explicitly disabled; enabled storage still needs a nonzero revision and successful durable check. Admission, cancellation and native ENV verification remain unchanged. The added regression failed against the old implementation; Release/headless+update passed 88/88 after the fix, including absent/disabled storage, unavailable revision, pending persistence and durable completion. Hardware acceptance remains separate; downstream Product evidence stays private.

## Current batch: Product-owned parameter management

2026-09-29, based on clean Framework `65bcdb2` on main. Inspection confirmed that the existing transaction engine already borrows Product descriptors and contains no transport/storage implementation. Extend that boundary rather than add another manager.

- Implemented: publish `meter_parameter_exchange_t`, retain the reference port alias, and document copied nonblocking handoff, independent backend ownership and durable completion obligations. Keep existing take/reply, authorization, timeout, retained-result and uncertain-write semantics.
- Implemented: reject concrete remote Parameter and local Settings descriptor declarations in contracts/core/runtime through the architecture guard, with positive and negative fixtures. Production definitions stay in Product packages; test-only synthetic catalogs are allowed. Guard limitations and review scope are recorded as GCR-009.
- Implemented: deterministic reference tests for a deferred storage owner, successful write/read, failed completion and independent catalogs with different permissions/ranges. Existing synthetic protocol tests continue to exercise the same public exchange.
- Validation: `cmake --build build-adaptation-demo -j 8` and `cmake --build build-adaptation-release -j 8` succeeded. Complete `ctest --test-dir BUILD --output-on-failure -j 6` passed Debug/UI 90/90 and Release/headless+update 86/86, including parameter, architecture, public-header, bilingual and assertion guards. Standalone guard fixtures passed 15/15. Windows GNU host evidence only; no firmware or physical storage/transport acceptance is claimed. Existing LVGL deprecation warnings in the update UI test remain outside this parameter change.
- Product decisions remain: actual parameter mappings, credential policy, persistence completion guarantees and untagged-response drain rules require Product evidence. No customer parameters, new persistence format, migration, worker or runtime registry is introduced.

## Behavior-preserving hardening

| Order / item | Scope and rationale | Exit evidence | Status |
|---|---|---|---|
| H-01 | Record the architecture and discrepancies; link this plan from AGENTS and the docs index. Correct M-03, M-11 and verified owner comments to match current code. | Bilingual gate, source review, no changed policy. | Complete |
| H-02 | Make `runtime/meter_periodic.c` locally readable: descriptive parameters, expanded branches, explicit phase comments. Add independent tests for publication rejection atomicity, encoder rejection/identity and exhausted identities. | Five new named cases pass on the original implementation; existing lifecycle assertions retained; full Product matrix passes; optimized objects identical. No signature, layout, arithmetic, deadline or policy changes. | Complete |
| H-03 | Establish negative fixtures for architecture/ownership guards (M-07), then tighten relative-include handling. Keep every previous gate and scope. | Fixtures reject deliberate violations, permit valid includes and fail clearly on missing scan anchors; CTest/CI registers them. Record gate changes under governance rules. | Complete |
| H-04 | Ensure C test setup/checks cannot disappear under NDEBUG (M-08). | Debug and Release both reject a deliberate failing test; test-only enforcement; all Product matrices pass. | Complete |
| H-05 | Document native shared-state/lock ownership and add incremental startup/stop fault-injection seams (M-01/M-05). No worker movement or synchronization change. | Tests cover every initialized resource and acknowledgement, queue rejection and blocked durability; distinguish deterministic stubs from target results. | Portable characterization complete; D-03/target evidence open |
| H-06 | Inventory analyzer coverage and extend handwritten first-party scopes in reviewable groups (M-06); exercise documentation links and generator failure paths (M-09). | Scope artifact reconciles selected sources and compile commands; analyzer findings repaired or explicitly pending human review. | Explicit scope added; CI/target and remaining tooling follow-up open |

H-01/H-02 retain their original verification below. The 2026-09-28 continuation pressure-tests these priorities against an external real-product requirement set. For each batch: read affected contracts, characterize before editing, preserve external semantics, run directly relevant tests and the selected Product matrix, and record limitations. Avoid broad formatting churn or new abstraction layers.

## Requirement-driven continuation

Only reusable conclusions belong in this public repository. The private requirement documents were read as evidence, not copied into code, catalogs or fixtures. A local ignored evidence record retains document hashes and unresolved IDs. All P0 entries remain unresolved: A1/A2/A3, B1/B2/B3/B4, C1/C2, D4, E1, F3 and G1. Suggestions inside those entries are not approvals. Bus allocation, calibration prerequisites, counter policy and defaults also require confirmation even where the source labels them P1. No real-product protocol, password, fault table or configuration is introduced here.

| Capability | Current implementation / actual gap | Next reusable work |
|---|---|---|
| Product composition | Host and firmware now select one Product at build time; generic startup uses the composition contract. | D-02 bootstrap coupling closed; verify target budgets/adaptation separately. |
| Two CAN networks and periodic RX/TX | Frame and route keys already contain bus/format/ID; periodic definitions accept independent periods. Board topology/bitrate remains fixed by integration. | F-03: explicit overlapping-ID, timeout/recovery and 20/50 ms synthetic regressions. Physical timing and bus assignment remain unverified. |
| Value/source/freshness | Core already preserves value/source when VALID becomes STALE and starts UNKNOWN. ERROR may replace an invalid value; there is no separate last-good history. | Reuse Core expiry. Decide separately whether last-good history and message-level supervision are required; never present UNKNOWN/STALE as valid zero. |
| Parameters | Core numeric IDs identify local persisted settings; command ledger and SDO scheduler do not supply a reusable owner-qualified parameter catalog/result boundary. | F-02: isolated App-owned parameter service using existing request identity/ledger; explicit request/attempt correlation and retained typed results. No existing setting or wire format changes. |
| Permissions | Existing auth profile is two static booleans; no expiring/revocable grant. | F-02: explicit permissions and session epoch checked at admission and dispatch; Product owns credentials. Existing UI policy unchanged. |
| Settings persistence | Defaults, checksums, namespace/schema and two-slot recovery exist; unknown schema blocks overwrite; there is no migration registry. | Reuse storage primitives; D-05 must approve migration mapping and persisted representation before adapters change. |
| Hour counters and distance | No instrument accumulation/checkpoint/reset service exists. | F-05: specify units, discontinuity, overflow, reset authorization and checkpoint acknowledgements before storage integration. Product supplies activation and cadence; no universal Flash interval. |
| Diagnostics | Domain, CAN, storage, trace and periodic counters already separate several causes; generic rejection counters still conflate some reasons. | F-02 adds typed parameter outcomes. D-01 keeps product health transitions explicit; later classify remaining ingress/contract failures without mapping all rejection to DEGRADED. |
| Controller profile and capabilities | Product provides static capability flags and source policy; there is no atomic dynamic profile/capability publication contract. | F-06: specify profile-generation invalidation and normalized capabilities; Product maps brand/feature bits. |
| Calibration | Product App workflow exists; no reusable calibration lifecycle over fresh measurements and parameter results. | F-07 follows F-02: characterize prerequisites, freshness, write uncertainty and optional readback; no LVGL transaction logic. |
| Identity/version | Diagnostic build identity exists, but Product/update/controller identities have separate representations; unsupported reads need explicit results. | F-08: propose a read-only provider shared by Product consumers before changing CANopen/update integration. |
| CANopen maintenance | Mixed provides standalone SDO client; production-maintenance server is not part of this framework. | Separate transport adaptation after confirmed channel/OD requirements; do not infer NMT/Heartbeat or introduce a full DTC system. |

Current bounded delivery: F-02 parameter/permission contracts and deterministic tests; F-03 reuse/negative characterization; H-03 guard negative fixtures and H-04 assertion enforcement where needed for those tests. F-02 remains opt-in internal code, with no production backend binding. Firmware selection, persistent schema/migration, vehicle health policy and worker/timing changes remain explicit decisions. Further capability work stays in this plan rather than a competing roadmap.

## Architecture decisions

| Item | Proposed next action | Authority / state |
|---|---|---|
| D-01 rejection policy | Review actual counters and decide whether semantic validation rejects affect mode. Until decided, document current behavior. | Product/maintainer; open |
| D-02 firmware Product selection | One build-time Product owns static storage and locale setup; Demo and Reference-B compile independently with unchanged default behavior. | Composition coupling closed; target adaptation evidence open |
| D-03 lifecycle failures | Decide cleanup/restart/durability failure semantics before changing ownership or shutdown guarantees. H-05 may characterize current behavior independently. | Maintainer; open |
| D-05 persistence migration and counter semantics | Retain unknown-schema refusal and two-slot recovery; review known-schema transforms, counter units/activation/reset/checkpoints and power-loss budget. Do not write unconfirmed mappings or defaults. | Product/maintainer; open |
| D-06 Settings/Parameters vocabulary | Keep local authoritative Settings names/MSP2 and remote owner-qualified transactions separate; no public rename/migration. | Compatibility retained |
| D-07 Product App/UI binding | Review confirmed backend correlation/drain, authentication and orphan panel/profile lifecycle before native binding. | Product/maintainer; open |
| Governance enforcement | Review existing TAD-001 and configure/verify required checks as described in compliance status. No agent approval or blanket exception. | Human reviewer/repository administrator; pending |

Open rows require the listed human/Product decisions. D-02 is the bounded implementation explicitly authorized for unchanged build-time composition; D-06 records retained compatibility. No CAN/protocol behavior, timing guarantee, thread ownership, storage format, update trust/recovery or diagnostic/safety policy changes are authorized by this plan.

## Product feature work

Real vehicle protocol integration, customer UI/assets, product diagnostics/safety rules and update authentication/recovery are separate Product work. D-04 in the assessment defines the requirement gap. Do not import private content into this public reference tree. Host Reference-B/Mixed success does not close firmware adaptation or dual-bus acceptance.

## Current batch verification

Baseline fresh Debug headless + update build: **42/42 CTests PASS**, Windows GNU 16.1.0 / Python 3.13.15. Reproduction from the application root:

```sh
cmake -S . -B build-maintainer-audit -G Ninja -DMETER_BUILD_UI=OFF -DMETER_ENABLE_UPDATE=ON -DCMAKE_BUILD_TYPE=Debug
cmake --build build-maintainer-audit -j 8
ctest --test-dir build-maintainer-audit --output-on-failure
```

Post-change results for the uncommitted H-01/H-02 batch based on `81e5083`:

| Configuration / check | Result | Local evidence |
|---|---|---|
| Debug headless + update | 42/42 PASS | `build-maintainer-audit/Testing/Temporary/LastTest.log` |
| Fresh Demo SDL | 44/44 PASS | `build-maintainer-audit/demo-ctest.log`, `build-maintainer-demo/test-results.xml` |
| Fresh Reference-B SDL | 34/34 PASS | `build-maintainer-audit/refb-ctest.log`, `build-maintainer-refb/test-results.xml` |
| Fresh Reference-Mixed SDL | 33/33 PASS | `build-maintainer-audit/mixed-ctest.log`, `build-maintainer-mixed/test-results.xml` |
| Python host utilities, including real native packaging tools | 100 passed, 9 physical HIL skipped | `build-maintainer-audit/python-venv-tests.log`, `python-venv-results.xml` in that directory |
| Original periodic implementation with final characterization tests | PASS | `build-maintainer-audit/periodic-original-characterization.exe` |
| Optimized periodic object before/after | Byte-identical with GNU 16.1.0, C11, O2, no debug info | `build-maintainer-audit/periodic-before.o`, `periodic-after.o` |
| Formatting and repository guards | Changed periodic files satisfy clang-format; architecture, ownership, public headers/clean and 26 bilingual pairs pass | CTest logs and final local checks |

For the SDL matrix use the same Debug CMake/build/CTest sequence with UI enabled, `METER_PRODUCT_ROOT` set to `products/demo`, `examples/reference-b` or `examples/reference-mixed`, and separate build directories. On this Windows host SDL2 was selected through `SDL2_DIR` and its runtime directory added to PATH for CTest; tests use the dummy video driver. These automated checks do not constitute a human visual review.

The initial system-Python attempt had 96 passed, 1 failed (missing `isotp`), and 12 skipped; it is retained in `build-maintainer-audit/python-tests.log`. The successful rerun used the existing test virtual environment with the pinned HIL/OTA requirements and explicitly selected `METER_OTA_INSPECTOR`, `METER_OTA_CPIO` and `METER_OTA_MKENVIMAGE`. No test or threshold was weakened to obtain the final result. Set those variables to the current build/native tools and run:

```sh
python -m pip install -r tools/hil/requirements.txt -r tools/ota/requirements.txt
python -m pytest -q -ra
```

Objects share SHA256 `9bf4e998b6baedb2748863bf7f5eeb668dbf059852ad716d8341c0d09f83cfc9`. This is evidence for this host compiler, not a target timing measurement. Raw build/configure logs and a result manifest remain in the ignored `build-maintainer-audit` directory; the table records the durable repository summary. No current-tree firmware/HIL, Linux analyzer or sanitizer acceptance is implied. Existing [source-bound hardware evidence](../testing/validation.md) retains its original identity.

## Requirement batch outcome

The preceding audit is committed separately as `c3ddb09`; the SDK pins it in `81e0408b`. This continuation uses `codex/product-framework-hardening` in both repositories. No sibling private-application changes are included. The original H-01/H-02 evidence above remains a historical record.

| Item | Delivered scope | Remaining boundary |
|---|---|---|
| H-03 | Real-CLI negative fixtures; local quoted include resolution and known Product roots; missing/duplicate/reversed scan anchors fail clearly | Lexical checks are not transitive call, macro expansion or concurrency proof; GCR-002 review remains pending |
| H-04 | Test-only NDEBUG removal and assertion failure witness; full Debug matrix and fresh Release pass | Production optimization/flags unchanged; future test naming/compiler changes require review |
| F-02 | Owner-qualified numeric catalog, fail-closed unconfirmed entries, permission grants, existing ledger, bounded read retry, typed retained outcomes | Opt-in internal seam only; no production adapter, credentials, protocol encoding, persisted schema or UI binding |
| F-03 | Overlapping bus/format identity; UNKNOWN/STALE/ERROR, value/source retention and recovery; independent 20/50 ms schedules | Existing implementation characterized; no physical bus/timing/controller acceptance |

The [parameter contract](../product/parameter-service.md) makes ownership, readiness, backend quarantine, cancellation uncertainty, float representation and deadline limitations explicit. Synthetic tests cover result retention, wrong owner/session/serial/operation/attempt, permission lifetime, clock wrap and exhaustion. A successful local token comparison never substitutes for wire-response correlation.

| Current-tree check | Result | Evidence |
|---|---|---|
| Debug headless + update | 46/46 PASS | `build-maintainer-audit/build-maintainer-audit-final-ctest.log` |
| Debug Demo SDL | 48/48 PASS | `build-maintainer-audit/build-maintainer-demo-final-ctest.log` |
| Debug Reference-B SDL | 38/38 PASS | `build-maintainer-audit/build-maintainer-refb-final-ctest.log` |
| Debug Reference-Mixed SDL | 37/37 PASS | `build-maintainer-audit/build-maintainer-mixed-final-ctest.log` |
| Fresh Release headless + update | 46/46 PASS | `build-maintainer-audit/build-maintainer-release-final-ctest.log` |
| Python host tools | 107 PASS, 9 physical HIL skipped | `build-maintainer-audit/framework-python.log` |
| Documentation / formatting / scope | 27 bilingual pairs, public-clean, headers, architecture and ownership PASS; changed C satisfies clang-format | CTest XML per build plus local final checks |

Release reproduction uses the headless configuration above with `-B build-maintainer-release -DCMAKE_BUILD_TYPE=Release`, followed by build and CTest. Existing SDL build directories were incrementally rebuilt. GNU 16.1.0 and Python 3.13.15 on Windows supplied this evidence; Linux sanitizer/analyzer jobs and current-tree firmware/HIL remain NOT_RUN. The first guard-fixture run caught Windows path separators; diagnostics were normalized and the full matrix rerun without removing checks.

Next safe batches are H-05 owner/lifecycle characterization and F-07 calibration workflow specification over F-02; F-05 counters, F-06 profile/capability publication and F-08 identity provider remain designed work, not implemented features. D-01/D-02/D-03/D-05 and all unresolved requirement entries stay open. Real-product adaptation requires confirmed descriptors plus backend correlation/drain tests before any production parameter writes.

The two new runtime sources additionally pass host GCC `-fanalyzer -Wall -Wextra -Werror`; this does not replace CI Clang/Cppcheck or target analysis. `build-maintainer-audit/framework-evidence.json` records tool version and source SHA256; final matrix XML is `final-framework-results.xml` in each build.

## App integration continuation

Accepted baseline: `7ed094f` on `codex/product-framework-hardening`; host/quality green reported by the maintainer. Continue this plan, not a new audit. Current order follows dependencies: characterize native startup/shutdown and failure ownership first; prove build-time firmware composition with independent Products; then exercise opt-in parameter Application integration and copied presentation values with synthetic backends. Add Release/headless CI and extend explicit handwritten analyzer coverage without reducing existing scopes.

Continuation outcome: H-05 portable lifecycle characterization, D-02 single-Product composition and the F-02 App/presentation witness are implemented below. D-03 and D-07 preserve unresolved production policy. No private wire encoding, credentials, physical timing or persisted formats changed.

### H-05 lifecycle evidence

Fresh-process fault injection executes all 21 native resource initialization failures, App startup failure, TX0/TX1/Protocol startup failures, NVM startup failure and staged shutdown durability/worker/UI acknowledgements. Existing IPC tests retain queue saturation and copied publication coverage. The same 28 lifecycle/IPC tests passed before and after extracting the App-owned shutdown handshake into one named operation. No worker, lock, queue, delay or timeout policy moved. Host object hashes differ; no binary identity or target timing claim is made.

D-03 evidence: partial initialization retains already-created native objects while leaving `initialized` false; retry would revisit them. App-thread startup failure leaves the runtime initialized/FAILED with no App owner to drive shutdown. Completed shutdown is one-shot. A durability or worker acknowledgement that never arrives can wait indefinitely; the 5-second diagnostic does not authorize forced teardown. Decide bounded failure/reboot/retry policy with RT-Thread resource semantics before changing these paths. Tests characterize retention, not endorsement of retry. Optional OTA worker lifecycle and actual RT-Thread scheduling remain outside this stubbed fixture.

Release/headless + update now has a CI build and CTest step. Local full-suite results and expanded analyzer provenance are recorded as subsequent batches complete.

### D-02 build-time composition outcome

The authorized build-time direction is implemented without changing the default Demo runtime. `contracts/meter_firmware.h` binds four independent static owner stores and a UI-owner locale initializer. Demo owns the same storage types/capacities and calls the same locale initializer at the same startup point. Reference-B supplies independent signal-only storage and its existing UI translation registration. Generic `main.c` contains no Product implementation includes; a negative ownership fixture protects this boundary. No runtime plugin loader, protocol route, worker, clock, storage schema or public Product struct changes.

SCons selects `METER_PRODUCT_ROOT` (default `products/demo`) through `tools/firmware_product.py`; relative paths resolve from the application root. The selector rejects missing/escaping/duplicate sources and unsupported feature closures. Reference-Mixed is deliberately not firmware-enabled: its SDO closure requires a separate board integration decision. Inspect selection with `python tools/firmware_product.py --product-root examples/reference-b`. This command does not build or flash.

Demo and Reference-B each compile the same generic firmware entry and link/run their selected composition against the real catalog, core and LVGL libraries. Tests verify capacity, independent owner arrays and copied snapshots, plus locale setup. This closes the Demo-specific bootstrap coupling and gives D-02 concrete review evidence. Target linkage, memory budgets, display/CAN board adaptation and physical acceptance remain unverified; host success does not enable a production release.

### Local Settings and remote Parameters

| Existing concept | Authority / meaning | Compatibility boundary |
|---|---|---|
| `meter_parameter_def_t`, snapshot parameter arrays | App-owned local authoritative Settings; catalog ID indexes local engineering values | Keep names/layout and MSP2 records; never treat an array write as a remote write |
| `METER_ACTION_PARAMETER` | UI intention to change a local setting, checked and applied by App | Existing admission/persistence semantics unchanged; no remote transaction dispatch |
| `meter_auth_profile_t` | Static Product policy for local settings and vehicle control | Not a login, credential or expiring grant |
| `meter_parameter_definition_t`, `meter_parameters_t` | Owner-qualified remote descriptor and retained transaction result | Confirmed synthetic descriptors do not authorize real wire mappings; no implicit snapshot/NVM mutation |
| `meter_authorization_t` | App-owned expiring/revocable permission grant for transactions | Product authentication supplies it; never persist credentials or infer them from static booleans |

D-06 compatibility disposition: retain existing public names and serialized representation. Their overlap is semantic vocabulary, not permission to merge storage or authority. New documentation and boundary examples use Settings for local values and Parameters for remote transactions. Any future public rename, migration or remote-to-local cache must first specify caller compatibility, record versioning, freshness and confirmation policy. No such migration is needed for this batch.

Every firmware image selects exactly one Product. The singular `METER_PRODUCT_ROOT` resolves one manifest and one `meter_product_get` / `meter_firmware_compose` implementation. Demo and Reference-B verification uses separate build directories and executables, never a combined firmware or runtime selector. Local composition batch: Demo 77/77 and Reference-B 67/67 CTests passed; evidence is `build-maintainer-audit/demo-composition.log` and `refb-composition.log`.

### App boundary and analyzer outcome

F-02 now has a synthetic App/transport/UI boundary witness in `examples/parameter-workflow`, described in the [parameter service guide](../product/parameter-service.md). It is test-only and does not add another firmware Product or alter existing runtime callbacks. D-06 retains local Settings names/format. D-07 remains a human/Product integration decision: confirm descriptors, authentication, backend quarantine and panel/profile lifecycle before production binding. No private encoding, credential or safety/health policy is selected.

H-06 adds seven explicit handwritten sources: diagnostics, trace, common routing, diagnostic commands, portable RT-Thread adapter, native execution owner and reference parameter App. `tools/analyze_handwritten.py` records source SHA256, actual analyzer version, command, result and include context; existing portable/generated scopes remain. Local Cppcheck passed all seven. Native analysis uses host RT-Thread stubs with OTA disabled; Clang-tidy/Linux CI and target compiler analysis are not claimed as run locally. The host workflow retains existing jobs and adds Release/headless + update and the explicit scope analysis. GCR-003 records the gate changes.

Remaining lifecycle boundary: partial init retention, failed App startup and one-shot STOPPED behavior are characterized but intentionally unchanged under D-03. State metadata uses `state_lock`; periodic publication copies use `tx_publication_lock`; App/UI snapshot copies use `view_lock`. No lock is held while the deterministic App fixture waits for another owner's acknowledgement. Worker stacks, queues and semaphore storage remain static; App alone advances the stop handshake, while each worker and UI acknowledge their own completion. Optional OTA lifecycle and real scheduler/resource reclamation still need separate evidence.

### Integration batch verification

| Configuration / check | Result | Evidence |
|---|---|---|
| Debug headless + update | 75/75 PASS | `build-maintainer-audit/audit-integration-tests.log` |
| Debug Demo SDL | 78/78 PASS | `build-maintainer-audit/demo-integration-tests.log` |
| Debug Reference-B SDL | 68/68 PASS | `build-maintainer-audit/refb-integration-tests.log` |
| Debug Reference-Mixed SDL | 66/66 PASS | `build-maintainer-audit/mixed-integration-tests.log` |
| Release headless + update | 75/75 PASS | `build-maintainer-audit/release-integration-tests.log` |
| Python host tools | 112 PASS; 9 physical HIL skipped | `build-maintainer-audit/integration-python.log` |
| Seven handwritten sources | Cppcheck and GCC `-fanalyzer -Wall -Wextra -Werror` PASS | `build-maintainer-audit/handwritten-cppcheck.json`, `handwritten-gcc.json` |
| Repository gates | Bilingual, public headers/clean, architecture and ownership including new negative fixtures PASS | CTest `integration-results.xml` in each build directory |

Use the earlier CMake configurations, build each directory and run CTest; Release is headless with update enabled, while each SDL directory selects exactly one Product. `python tools/analyze_handwritten.py --tool cppcheck --output build-maintainer-audit/handwritten-cppcheck.json` reproduces local Cppcheck. CI omits `--tool` to require both Cppcheck and clang-tidy. GNU 16.1.0 / Python 3.13.15 on Windows produced the local results. The Shell stub's missing export reference was repaired after GCC exposed it; native lifecycle tests were rebuilt and rerun. No warning policy was relaxed.

Commits `f30ff95` and `e07f899` separate lifecycle characterization and single-Product composition. Current-tree Linux Clang/sanitizer/fuzz CI, target firmware linking, real controller interoperability, physical timing, UI visual acceptance and HIL remain NOT_RUN. The maintainer-reported green CI belongs to baseline `7ed094f`, not this batch. Next implementation should follow D-03/D-07 decisions or independent H-06 tooling work; this is not approval for wire encoding, credentials, storage migration or health-policy changes.

## Application services continuation

Active milestone: Product-owned, headless-testable presentation over the existing snapshot; normalized runtime profile generations; a bounded calibration Application workflow over the existing parameter service; classification without health-policy changes. These are additive opt-in services, not a second runtime or snapshot engine.

Order follows dependencies: migrate Demo projection first; specify/profile-test generation invalidation; prove calibration and retained outcomes with synthetic transport; make D-01/D-05 and identity-provider decisions concrete. Keep one build-time Product per firmware and all existing matrices. No native worker restructuring, private wire mapping, credentials, persistent counter implementation or product safety-policy changes.

Private requirements are evidence only. Source freshness, feature visibility, profile-dependent fault/version selection and capture-to-parameter calibration motivate the boundaries; unresolved source conflicts remain unresolved. Each batch records unchanged behavior, deterministic evidence and physical limitations below.

### Services batch outcome

Demo now uses a headless Product projection; profile generation, calibration and diagnostic classification are opt-in contracts. See [Application services](../product/application-services.md) for ownership, rebuild compatibility, D-01/D-05 decision tables and identity assessment. D-03 remains unchanged; D-07 production mapping/authentication/cache invalidation remain open. Persistent counters and customer adapters are not implemented.

| Check | Result | Evidence |
|---|---|---|
| Debug headless + update / Demo SDL | 79/79 and 82/82 PASS | build-maintainer-audit/build-maintainer-audit-services-test.log and build-maintainer-demo-services-test.log |
| Reference-B / Reference-Mixed SDL | 71/71 and 69/69 PASS | build-maintainer-audit/build-maintainer-refb-services-test.log and build-maintainer-mixed-services-test.log |
| Release headless + update | 79/79 PASS | build-maintainer-audit/build-maintainer-release-services-test.log |
| Ten explicit handwritten sources | Cppcheck and GCC analyzer PASS | build-maintainer-audit/services-cppcheck.json and services-gcc.json |
| Repository checks | 28 bilingual pairs; architecture, ownership, public headers/clean and negative fixtures PASS | services-results.xml in each build |

Host GNU 16.1.0 / Python 3.13 evidence only. Current-source Linux CI, sanitizer/fuzz, physical timing and UI visual acceptance are not implied. Initial Python run lacked can-isotp; retry uses pinned requirements in an isolated environment, with the initial missing-dependency failure recorded here. Board validation is recorded separately after committing source.

Python host tools: 114 PASS, 9 physical HIL skipped; build-maintainer-audit/services-python.xml and services-python.log. Native SDK cpio/mkenvimage supplied through the documented METER_OTA_CPIO/METER_OTA_MKENVIMAGE environment overrides; no packaging tests skipped.

### Services hardware closure and next work

Commits 1cf7aea (profile), 5342d4b (Product presentation) and 5cfa0bf (calibration/classification) are separate review batches. The committed candidate built for reference-board and passed 9/9 physical HIL after CAN OTA and source-verified reboot; see [validation](../testing/validation.md). The board retains services-a and both interfaces are released. Default runtime/protocol/timing/storage policies remain intact; new services still require explicit Product binding.

Next work belongs to this plan: review D-07 Product mapping, measurement/profile invalidation and authentication before real adapters; resolve D-01/D-05 policy tables before health changes or persistent counters; leave D-03 ownership/recovery semantics unchanged. Track inherited iso14229 documentation-submodule metadata as a dependency inventory issue. Current-head Linux CI remains unrun locally; no new roadmap, private wire mapping or automatic dependency repair is introduced.

### Product adaptation boundary follow-up

Continue from 67a378a without new framework mechanisms. The Demo projection, normalized profiles, calibration service and classifier already satisfy the foundation. The remaining reference UI leaked owner-qualified keys and full transaction results. Batch A moves address selection into the synthetic Product App and exposes semantic fields plus plain result values. No runtime/public contract or production firmware changes. Deterministic tests retain owner separation, panel/profile lifetimes, permission/drain rules and now distinguish valid zero from unavailable/rejected values.

Batch B now exercises the existing services together in the headless product-application-flow recipe: invalidate old measurements before profile publication, normalize feature visibility in App, capture only the matching acquisition generation, retain uncertain writes through profile replacement, and complete readback after backend drain. Keep all policy illustrative; D-01/D-03/D-05/D-07 remain open for production decisions.

Batch A verification: parameter App, public headers, architecture and negative guard fixtures pass on the fresh Debug/headless build; valid-zero and rejected-result copies are distinguished. Chinese explanatory comments are now required by AGENTS.md, with the new service contracts corrected in a separate comment-only commit. No current batch hardware result is claimed.


Batch B adds only a test-local Product App/ViewModel composition, documented in [Application services](../product/application-services.md). Mapping and policy remain explicit Product code; no new generic runtime interface or production worker is introduced. It demonstrates source-generation rejection, coherent invalidation before publication, retained uncertain old results, independent backend drain, capture stability, optional readback and permission-loss classification.

| Check | Result | Local evidence under evidence/adaptation/ |
|---|---|---|
| Debug headless + update | 80/80 PASS | headless.xml; final recipe rebuilt and rechecked in headless-recipe.xml |
| Release headless + update | 80/80 PASS | release.xml; final recipe rebuilt and rechecked in release-recipe.xml |
| Demo / Reference-B / Reference-Mixed SDL | 83/83, 72/72, 70/70 PASS | demo.xml, reference-b.xml, reference-mixed.xml |
| Python host tools | 115 PASS, 9 physical HIL skipped | python.xml; pinned requirements in isolated Python 3.13 environment |
| Handwritten analyzer baseline | Ten sources, Cppcheck PASS | handwritten.json and handwritten.log; scopes retained |
| Reference App and integration recipe | Cppcheck and GCC analyzer PASS | adaptation-analysis.json; source hashes, versions, commands and raw diagnostics |
| Format, public headers/clean, architecture and docs | PASS; 28 bilingual pairs | matrix logs, boundary-test.log; no check or threshold removed |

Toolchain: Windows, GNU 16.1.0 (WinLibs headless; MSYS2 SDL), CMake 4.4.3, Python 3.13.15, Cppcheck 2.21.0 and clang-format 23.1.1. SDL initially auto-selected an unprepared MSYS2 Python; initial failures remain archived and reconfiguration explicitly selects the pinned environment. Additional analyzer coverage found side effects inside new test assertions; calls were separated from assertions and rechecked without suppression. The first diagnostic capture also encountered mixed Windows output encoding; exact bytes are retained. The new fixture was rebuilt/retested after that source correction in both headless modes. These results do not claim current-head Linux CI, sanitizer/fuzz, rendered-UI acceptance or new hardware testing. The board was not accessed in this batch; prior services-a evidence remains bound to its original source.

The next Product work should supply confirmed mappings/catalogs, capability normalization, acquisition identity, authentication policy, ViewModels and UI composition. D-07 remains pending for actual transport correlation/drain and invalidation/authentication policy; D-01 for health consequences, D-05 for counter semantics and persistence, and D-03 for native recovery/wait contracts. None is implicitly approved by the synthetic recipe. Do not add another shared mechanism until a concrete adaptation gap demonstrates reuse and reduced complexity.

### Demo UI readability pass

The Demo UI now follows the reference 800x480 instrument geometry: a stable 53 px header, 372 px content canvas, and 55 px navigation band. Primary values retain a large value face; product labels, page titles, monitor rows, fault rows and settings controls move to larger text classes. Cards and status rows gain vertical breathing room, while the existing semantic presentation boundary, four-page navigation, translations and widget behavior remain unchanged.

The Chinese resolver uses the checked-in 20 px subset for label and value presentation, so this pass does not add an unreviewed font source or alter the font-generation contract. English uses the available LVGL 16/20/24 px faces. SDL captures and scenario/i18n/fixture tests are the visual and behavioral host evidence; no rendered hardware acceptance is claimed yet.

### Demo UI pagination pass

Monitor, fault and local-settings content now uses explicit two-page presentation within the existing 800x480 canvas. Monitor and fault rows are page-local and inactive rows are hidden; settings separates preference selection from numeric controls. Previous/next controls have disabled end stops and preserve the existing snapshot, action and translation contracts. The new deterministic UI test checks both languages, valid/stale/error values, retained page selection, bounds, end stops and rejected settings actions. This is host evidence only; board visual acceptance remains open.

The pagination batch is committed as framework `005e157` plus scenario coverage `973795f`; the SDK pins `973795f` in `cfda3cf0`. Debug Demo CTest passed 84/84, including the new `ui-pagination` test. A candidate built with the SDK Python 3.8/SCons entrypoint and was installed through the existing CAN OTA path as `demo-ui-20260929`; UART and CAN identity after reboot match the new framework source. The board display was not captured, so rendered page visual acceptance remains `NOT_RUN`.

### Demo product adaptation UI and authorization

The Demo now presents two separate monitoring functions: parameter telemetry and controller parameter settings. Both use Product presentation data and real translation tags; no mixed-language labels are assembled in UI code. Controller settings use a Product-owned synthetic adapter that exercises `METER_ACTION_PRODUCT`, owner-qualified parameter admission, expiration and result publication. It deliberately does not encode a private CAN protocol.

Settings are four pages with four local items on the user page, a PIN page, four administrator items and an instrument-version page. User PIN `1234` and administrator PIN `5312` are demonstration credentials only. Administrator items remain hidden until the expiring App authorization grant is valid. The version page obtains firmware/build identity where available and reports the host fallback explicitly.

The new `services/settings_app.c` owns authorization and parameter service state, publishes copied semantic values through the existing snapshot revision path, and leaves local authoritative Settings in `meter_core` separate from remote owner-qualified Parameters. `demo-settings-app` verifies wrong credentials, independent owners, invalid/out-of-range values, queued and applied results, logout, expiry, wrap-safe time and reset without altering the local settings array.

Host evidence: Debug Demo rebuilt after catalog/font regeneration; `demo-settings-app`, `ui-pagination`, `i18n-ui`, `sdl-smoke`, firmware composition, architecture and public-clean checks pass. Captured SDL pages are stored under `evidence/adaptation/settings-captures/`. The images are host evidence; touch behavior, rendered screen quality and keyboard interaction on the target board remain pending until the next OTA window.


Board validation 2026-09-29: OTA candidate demo-ui-auth-20260929 package SHA256 835DD979FB9E520D6E8BA6CF97B214D28484CD5B0962F8FF967A31EC32F4CD64; native build PASS, package preflight PASS, physical HIL 9/9 PASS on COM11/PCAN_USBBUS1. Before identity was demo-ui-20260929; after reboot the board reported demo-ui-auth-20260929, reference-demo/reference-board, CAN 500000, domain signals=25 parameters=10 faults=10, UI present/flush advancing, storage READY. Raw UART/CAN evidence is under evidence/ota/demo-ui-auth-20260929/board2/. Touch keyboard, PIN entry and rendered page visual acceptance were not exercised in this OTA batch.


### Demo instrument PDO continuation

The accepted UI baseline is 418a123. Add two explicitly synthetic, transmit-only Demo PDOs through the existing App publication and Product encoder, preserving the existing 0x3C0/50 ms and 0x2F0/100 ms frames. Keep the receive DBC/generator and Core contracts unchanged; a separate transmit DBC describes the new wire layout. These are cyclic process-data examples, not a CANopen NMT/SYNC/object-dictionary implementation or customer mapping.

Order: protect exact payload/freshness/counter and scheduling behavior with deterministic Product tests; add physical CAN receive/recovery checks; build a single Demo firmware, install through existing OTA, and retain identities, hashes and raw logs. Stay inside current static TX budgets. Preserve source sample times and invalidate stale data explicitly. Update this section with results; rendered target UI acceptance remains independent of CAN/HIL acceptance.

Host verification: Demo Debug 87/87 CTests, Release/headless + update 83/83 CTests, Python tools 115/115, and all eleven explicit handwritten Cppcheck sources pass. Evidence is under evidence/adaptation/ with demo-pdo, pdo-release, pdo-python-retry and pdo-analysis prefixes. The initial Release build exposed interpreter discovery after the identity-generation target; the bounded CMake fix resolves Python before constructing that command, including non-test builds. Initial Python failures also identified the new HIL inventory count and missing local inspector/tool paths; the inventory now includes the tenth PDO test and the full rerun uses the existing real inspector/cpio/mkenvimage. No thresholds were reduced. Firmware and physical evidence follow separately.

### Demo PDO board result

Candidate `demo-pdo-20260929` was built from framework 495175f and installed through the existing CAN OTA path. Package SHA256 is `65e1e55b9ae3c29f91ec9f58a567e0db6f93657e29745f0ff46554fcaf06b45c`; target OS image SHA256 is `30b952a1aa9e4098f619dbf89828245bdf0d7e75702f58d24d8a937d50fd22ca`. COM11 and PCAN_USBBUS1 reboot identity reported product `reference-demo`, board `reference-board`, firmware `demo-pdo-20260929`, platform `495175f`. Physical HIL is 10/10 PASS, including `test_demo_instrument_pdo`; XML, UART, CAN ASC and per-test captures are under `evidence/ota/demo-pdo-20260929/`. The build report records SDK pin 6560f24d and config restoration.

Host screenshot evidence shows the black 800x480 dashboard with larger Chinese text, green/red state contrast and separated navigation. It remains a host render; touch feel and target display visual acceptance are not claimed by HIL.

### Password page layout correction

The Demo password editor now follows the 参考项目 参考产品 reference composition within the existing 800x480 shell: a two-item settings rail on the left, a right-side title and back control, a wide four-digit masked field, and a four-row three-column numeric keypad. The keypad is a Product UI button matrix with explicit digit, backspace and confirm behavior; it has no LVGL keyboard focus dependency. The dark Demo palette is retained while the reference proportions and spacing are restored. `ui-pagination` captures `password-page.bmp` and checks the existing user/admin authorization path; the targeted Demo UI tests pass after the change. Target-panel visual acceptance remains a separate OTA check.


Password page OTA result: `demo-password-reference-20260929` was built from framework 341f7c3, packaged with SHA256 `2f7020107c5c2e48aa4b9adb9cd3326c656fbec28da2a393c0026810ae572a10`, installed and reboot-verified on the target-board. UART/CAN reported platform 341f7c3, board reference-board, LVGL 9.6.0. The board session was released after verification.

### Demo UI visual redesign

The visual review found that the earlier palette and navigation made the Demo resemble a generic software dashboard. This batch establishes one instrument-oriented visual language across the dashboard, monitoring, faults, settings and PIN pages: near-black canvas, restrained graphite panels, white and gray hierarchy, orange selection/action emphasis, and green/red reserved for vehicle state. The bottom navigation now combines a small symbol with a translated label, while the content and page controls keep their existing geometry and pagination contracts. The PIN rail uses the same orange active state as the main navigation. No protocol, snapshot, authorization or timing behavior changed.

Fresh SDL captures are under `evidence/adaptation/redesign-captures-3/`. `ui-pagination`, `demo-settings-app`, `i18n-ui` and `sdl-smoke` pass after the theme and navigation changes. This is host visual evidence only; target-panel color, touch feel and rendered hardware acceptance remain open for the next explicitly scheduled OTA batch.

Board validation 2026-09-29: candidate `demo-ui-redesign-20260929` was built from Framework `d63793e` and SDK `967d9ec0`. The OS image SHA256 is `a43cdc7b61c819f740c3f997e762878ad8125ca3be5c03648d64b79777f7df1f`; the CAN OTA package SHA256 is `35aff422d7d38031a342de555488098633b82484169b40d55e82cc5e1ac6c15d`. Host package preflight passed, the board accepted 1,106,944 bytes, activation and reboot completed, and post-reboot probe reported `reference-demo/reference-board`, version `demo-ui-redesign-20260929`, platform `d63793e`, SDK `967d9ec0`, state `IDLE`, error `0`, and zero queue rejects. Raw probe/download/activation/verification evidence is under `evidence/ota/demo-ui-redesign-20260929/`. This confirms firmware composition and OTA identity; rendered target-screen visual acceptance still requires a photo or capture from the panel.

### Demo UI density refinement

The next visual review found two remaining issues: pagination controls consumed too much visual space, and settings still read as a row of unrelated controls. Pagination now renders a compact `< 1/4 >` style indicator while retaining independent 64 x 44 touch targets at both ends. Settings uses a persistent left category rail and a single right content panel; long English category labels wrap inside their touch targets. The dashboard removes decorative rounded cards and direct section captions, and enlarges the five vehicle-state icons. Existing page bounds, translations, actions and snapshot semantics remain unchanged. New SDL captures are under `evidence/adaptation/ui-refine-captures/`.

Board validation 2026-09-29: candidate `demo-ui-refine-20260929` was built from Framework `8d3a494` and SDK `fd484e09`. The OS image SHA256 is `414e3609ba63504297f84e18a0c29afe92bafcf8977fa9253ef810e2a624d941`; the CAN OTA package SHA256 is `cd9aa864476c40663090d87a3a423349ce29d4ce9530f3074d728d307e0e6b7f`. Host preflight passed, the board accepted 1,106,944 bytes, activation and reboot completed, and post-reboot probe reported version `demo-ui-refine-20260929`, platform `8d3a494`, SDK `fd484e09`, state `IDLE`, error `0`, and zero queue rejects. Raw evidence is under `evidence/ota/demo-ui-refine-20260929/`. The target display was not photographed, so visual acceptance remains host-capture evidence plus board identity/boot verification.

### Demo instrument footer and counter pass

The dashboard footer now fills the 800 px canvas with four equal 200 px touch tabs. The prior rounded gaps are removed while the icon and translated label remain centered in each 55 px touch area. The dashboard also presents Demo mileage and work-hours readings beside the vehicle status strip. Mileage is explicitly a synthetic reference value; work hours preserve the snapshot value and validity state, including valid zero, stale and error. The five vehicle indicators are icon-only controls with enlarged 2x source assets; their green, red and warning colors still communicate active, inactive and unavailable states.

Deterministic presentation and pagination tests cover counter validity, continuous navigation bounds, icon-only labels, enlarged icons and footer geometry. Host captures are under `evidence/adaptation/ui-mileage-captures/`; the full Debug suite passes 87/87. This batch has not been accepted as a target-screen visual result; the next OTA identity and panel capture must be recorded separately.

### Demo instrument footer OTA result

Candidate `demo-ui-footer-20260929` was built from framework `ca5a02b` and SDK `b082da95`. The OS image SHA256 is `b97a3e575db697646c1d4d3f25eb10100435f78c3737ce91d54ce7e92e44f067`; the CAN OTA package SHA256 is `3f914781193537a4b140358eda672ad8f62c0a6b231a82786adc61469fe1b5af`. Host preflight passed. Maintenance admission, download, activation and reboot completed on COM11/PCAN_USBBUS1; the board accepted 1,106,944 bytes with zero queue rejects. Post-reboot probe reported `reference-demo/reference-board`, version `demo-ui-footer-20260929`, platform `ca5a02bd940b3a5575d29796323044bafc3a9b57`, SDK `b082da95f8bdd646951d38badf22117e961922f6`, state `IDLE`, error `0`. Raw evidence is under `evidence/ota/demo-ui-footer-20260929/`. No target-panel photograph was captured, so visual acceptance remains host capture plus board identity/boot verification.

### Demo settings screen compact pass

The settings page now follows the reference forklift composition more closely: one title, a four-item category rail, and one focused content panel. Explanatory note text, demo credential/session headings and the long administrator lock sentence were removed from the screen. User settings retain only four actionable rows; password access retains the two entry actions, login state and sign-out; administrator protection uses a compact warning icon until authorization is granted. Existing authorization, local Settings ownership and controller Parameter behavior are unchanged.

Host captures are under `evidence/adaptation/ui-settings-compact-3/`. The four settings pages and password flow remain covered by `ui-pagination`, `demo-settings-app`, `i18n-ui` and `sdl-smoke`; the full Debug suite passes 87/87. No target-panel visual result is claimed in this batch.

### Demo settings compact OTA result

Candidate `demo-ui-settings-20260929` was built from framework `09c50cd` and SDK `9187a19d`. The OS image SHA256 is `8255dce099b1ab912fb3dd205d3250007fdd51d1e62b6af7ac8fc687a56bdcea`; the CAN OTA package SHA256 is `ee1a54e1ece8bc84b7105a3f26c87c004c53ddf366f0964e0ac05cf078cab8b6`. Host preflight passed. Maintenance admission, download, activation and reboot completed on COM11/PCAN_USBBUS1; the board accepted 1,106,944 bytes with zero queue rejects. Post-reboot probe reported `reference-demo/reference-board`, version `demo-ui-settings-20260929`, platform `09c50cd188ade20b27ef6921c49f1f8ed4ccdc02`, SDK `9187a19df878ce1ebeb1f4af05992290b1871411`, state `IDLE`, error `0`. Raw evidence is under `evidence/ota/demo-ui-settings-20260929/`. No target-panel photograph was captured.

### Unified monitor/settings layout

The Demo monitor page now uses the same composition as settings: a left vertical category rail and a right single-column item panel. Parameter telemetry and controller parameter settings are selected by two centered left tabs; readings and editable values use aligned label/value rows on the right. The monitor row density was adjusted for all catalog entries without changing page count, authorization, snapshot or transaction behavior. The deterministic UI test now checks the tab and right-panel geometry in both language passes. Captures are under `evidence/adaptation/ui-monitor-settings-unified/`.

### Four-item monitor pagination

Monitor telemetry and fault lists now show four entries per page, matching the four-item settings pages. The monitor catalog therefore uses deterministic four-row pages; controller parameter editing remains a single right-panel view. Page counts and end stops are derived from catalog sizes, and no signal, authorization or transaction contract changes.

### Full-width left-tab and spacing refinement

The monitor and settings rails now attach to the page's left edge and form continuous, gap-free 200 px columns. Their tabs fill the available content height without rounded gaps. Right-side monitor rows use larger label/value faces and wider spacing; the four-item page rule remains enforced. The full SDL capture set, including dashboard, monitor pages, fault pages, settings pages and password page, is under `evidence/adaptation/ui-left-tab-full/`.

### Full-width left-tab OTA attempt

Host SDL verification passed and the complete capture set is stored under `evidence/adaptation/ui-left-tab-full/`. Candidate `demo-ui-lefttab-20260929` was built from framework `31d8ac6`; the OS image SHA256 is `e3746fe21833217bbe6e1c6ac3b778e84075b6a96970694838b27c63e4869375` and the OTA package SHA256 is `ae0e3736862fad99989f40d1cf7c64502464afde335a2af99c4014207a15a011`. Host package preflight passed. The board accepted the manifest and first 512-byte block, then returned UDS `RequestOutOfRange (0x31)` on the next transfer. Abort, maintenance off/on and a board reboot were attempted before retries; the same response persisted. Raw attempts are under `evidence/ota/demo-ui-lefttab-20260929/`. The board remains on `demo-ui-settings-20260929`; no OTA success is claimed for this layout.

### Compact left-tab visual refinement

The monitor and settings category controls remain aligned to the left edge with continuous 64 px touch targets. The settings rail is now a transparent container; only the four buttons draw their selected or inactive state, so the rail does not create an oversized background field. Monitor and settings still share the same geometry, typography, translations and four-item pagination rule. Fresh SDL captures are under `evidence/adaptation/ui-left-tab-compact/`; targeted UI tests pass and the existing full-suite baseline remains 87/87.

### Shared category-menu styling

Monitor and settings now use one `demo_theme_menu_button` rule for category controls: 200 x 64 px geometry is owned by each page, while radius, border, text color, selected orange and pressed feedback are shared. Settings no longer overrides menu colors directly, so both left menus have identical visual states without adding a rail background. Existing navigation and authorization behavior is unchanged.

### Unified item and password controls

Right-side settings rows, category menus and the password editor now share the Demo theme's dark surfaces, orange selected state, muted labels, border treatment and spacing rhythm. Password entry uses the same dark field and themed keypad instead of a standalone white control; the left password choices use the shared category-button rule. Authentication behavior and keyboard event handling are unchanged. The compact SDL capture set was regenerated after the change.

### Flat right-side item lists

Right-side monitor, controller-parameter and settings entries no longer use nested rounded rectangles. Their surfaces are transparent with a single bottom separator, preserving the dark canvas and making the four-row rhythm easier to scan. Category labels use single-line circular scrolling when a translation is longer than its touch target. The four settings categories continue to open as independent pages from the left menu; authorization and editor behavior are unchanged. SDL captures were regenerated under `evidence/adaptation/ui-left-tab-compact/`.

### Settings routing and state presentation

The Demo now exposes two settings routes: User Settings and Administrator Settings. The former includes a clickable instrument-version item that opens the version detail view and returns through its back control. Selecting Administrator Settings while unauthorised opens the administrator PIN editor first; the administrator page is shown only after the existing authorization result permits it. The former standalone Password category was removed. The settings and monitor left rails use filled background regions, while monitor values and labels use white text for stronger contrast. SDL routing captures are under `evidence/adaptation/ui-routing-admin-version/`.

### Admin/version routing OTA boundary

Host build and the full 87-test suite pass for the two-route settings change. A new board candidate build was attempted as `demo-ui-routing-20260929b`; the SDK wrapper restored `.config`, but SCons stopped before image generation because the SDK mkimage configuration returned a missing cluster size (`int(None)`) and the selected environment could not resolve the SDK Python helper. No OTA package was produced and no board state was changed. This remains a build-environment blocker; the prior board transfer `0x31` evidence is unchanged.

### Settings layout correction and OTA diagnosis

The routing capture exposed two layout defects: menu labels used left alignment inside wide touch targets, and the version entry overlapped the last user-setting row. Labels now use centered alignment, the five user-setting rows use a compact vertical rhythm, and the version entry stays inside the content panel. Monitor readout labels and values remain white. The OTA failure is independent of this UI change: the SDK wrapper reached SCons but the current SDK `.config` lacks the mkimage cluster-size value and the helper Python alias is unresolved, so no image was produced. The earlier successful OTA path used a complete board configuration; restore that target configuration before retrying.

Open decision: CAN bitrate persistence is not enabled by this layout-only correction. The current Demo CAN-rate row is an in-memory administrator preference; making it restart-effective requires extending the retained settings schema and boot-time CAN initialization contract. Keep this as a bounded follow-up rather than silently changing the persisted format.

### Five-item user settings page

The User Settings content panel now presents five compact rows at the existing font size: speed units, language, brightness, speed limit and instrument version. The rows fit within the panel without overlap; the version row still opens its independent detail page. Administrator routing and the existing CAN-rate entry remain unchanged pending the retained-settings extension decision.

### Restored SDK Python/SCons build environment

The board wrapper now uses the SDK-bundled Python 3.8 executable `tools/env/tools/Python38/python3.exe` together with the bundled SCons 3.1.2 library. Python 2.7 is retained only for legacy SDK utilities; it cannot run the current application SConscript because `importlib.util` is required. Candidate `demo-ui-routing-20260929` rebuilt successfully with this environment, produced the reference-board image and OS ITB, and restored `.config` byte-for-byte. Evidence is under `evidence/ota/demo-ui-routing-20260929e/`.

### Demo settings routing OTA result

Candidate `demo-ui-routing-20260929` was rebuilt with the restored SDK Python 3.8/SCons environment and packaged with host integrity PASS. The OS image SHA256 is `ae80d1cedd4969e1086cfaa2ff722a7ecd0a5c6bd87c04a956f64d3b5e60d0ad`; the OTA package SHA256 is `297439da36e89c2d872ca72993a0fc49f504e0759c5f0f4aa2fdc723e1c2c1c3`. Maintenance admission, download of 1,111,040 bytes, activation and reboot completed on COM11/PCAN_USBBUS1. Post-reboot identity reported `reference-demo/reference-board`, firmware `demo-ui-routing-20260929`, Framework `d55321b`, SDK `9db13a6c`, LVGL `9.6.0`, state `IDLE`, error `0`, queue rejects `0`. Raw evidence is under `evidence/ota/demo-ui-routing-20260929/`.

### Tab-local pagination and unified typography

Pagination controls now live at the upper-left of the active right-side content panel, so the bottom navigation remains dedicated to page routing. Monitor rows begin below the control and use the shared five-row layout target; fault rows keep their own Tab-local pager. Settings title and user-setting labels use the same larger face and spacing rhythm as the monitor panel. The existing instrument-version detail route keeps its back control.


### Five-row pages and independent setting details

The Demo monitor, fault and settings content now uses a shared five-row layout target: 56 px rows with 60 px rhythm and bottom separators. Monitor and fault pagination remains local to the active Tab; the compact pager is anchored at the upper-right of the content area. User settings expose five visible entries, and speed units, language, brightness and speed limit each open a separate detail route with a back control. The list is a presentation of the current snapshot; opening a detail page never sends an action. Administrator entries use the same row geometry and open the same detail surface after authorization. Host SDL captures and deterministic route/action checks are under `evidence/adaptation/ui-five-row-details/`. This batch does not enable CAN bitrate persistence or alter any retained format.


### Five-row candidate OTA boundary

Candidate `demo-ui-five-row-20260929` was built from Framework `263b87c` with the restored SDK Python 3.8/SCons environment. The OS image SHA256 is `170ff976ff525a2c8e720e0bc729d3e49d953afa6833c19591dcff6e69449cd7`; the package SHA256 is `fdb20b2d82824dc95dcc22352c591146e9266ee5284425889c5ea79770c80a46`. Host package integrity passed. The target accepted the first 512-byte block, then returned UDS `RequestOutOfRange (0x31)` for the next TransferData request. The transfer was aborted and maintenance recovery was attempted; `meter info` confirms the board remains on `demo-ui-routing-20260929`. This batch therefore has no OTA success claim; raw UART/CAN evidence is under `evidence/ota/demo-ui-five-row-20260929/`.


### OTA alignment root cause and successful retry

The failed candidate used an OS payload of 1,107,968 bytes, which is 2 KiB aligned but not divisible by the native AIC backend write block of 4 KiB. The backend rejected the session when the first FIT metadata and image block were committed, surfaced over UDS as `RequestOutOfRange (0x31)`. The previous successful candidate used a 1,110,016-byte, 4 KiB-aligned OS payload. The OTA packer now defaults to 4 KiB OS padding and has a regression test for the reference-board board contract.

After rebooting the target, the corrected package was preflighted and installed: 1,111,040 bytes received, candidate verification passed, activation and reboot completed, and post-reboot identity reported `demo-ui-five-row-20260929`, Framework `263b87c`, SDK `5bef2d47`, LVGL `9.6.0`, state `IDLE`, error `0`, and zero queue rejects. Package SHA256 is `10cc2260307ea3fd05f41a0debcf1c6d9c7e1a890e96662a27c1b83395828016`; raw evidence is under `evidence/ota/demo-ui-five-row-20260929/retry-fixed/`.


### Retained board settings integration

User-authorized on 2026-09-29: implement real brightness and retained CAN bitrate using the existing App/NVM owners. This closes the earlier CAN persistence decision; it is a deliberate storage/boot extension, not a behavior-preserving cleanup. One firmware still contains one Product. No customer protocol, assets or credentials are imported.

- MSP3 uses header byte 7 for the stable 125/250/500 kbit/s selection (0/1/2), and Demo declares FMP2 schema 3. Per the maintainer clarification, there is no legacy-record reader or migration. Retain stable parameter IDs, checksum and two-slot durability. Reject old or corrupt records without partial application or automatic erase; explicitly initialize the authorized development-board settings region before validating the new record.
- App waits for the asynchronous initial NVM result before starting CAN workers, then latches an immutable rate for both reference-board buses. Blank/invalid media uses validated defaults while preserving NVM error/repair policy. Slow I/O keeps CAN closed; stop remains cancellable. A configured rate change is authorized by Product, changes the snapshot revision and is saved by the existing debounced service; live CAN/OTA traffic stays at the boot rate. Restart only after the durable revision catches up.
- The optional Product local_action callback now receives a borrowed mutable App snapshot. Update out-of-tree callbacks to accept the snapshot first; never retain it or perform I/O. UI and shell both reach this callback through App-owned queues. The shell product intent route does not bypass authorization. Remote Parameters remain transport-neutral and separate.
- Board PWM3 applies brightness 10..100 through a 20000 ns period and 2000 + 180 * value ns pulse, with register readback and bounded one-second retries. The board adapter owns electrical mapping; App applies after retained load and each accepted change. Host UI keeps full opacity. Unknown dashboard values retain -- without duplicate NO DATA labels; stale/error indications stay visible.

Execution order: finish deterministic format rejection, authorization, PWM failure/readback and delayed-boot tests; run Debug/Release and target build; verify OTA identity, retained brightness and CAN reboot behavior on hardware; then consolidate this development branch by functional changes with recoverable old refs and unchanged source trees. Do not attribute old hardware captures to rewritten commit IDs. Current hardware validation for this batch: NOT_RUN.


### Functional commit grouping

The retained-settings change is intentionally kept as three reviewable commits after accepted OTA baseline `97ab6e3`: `0695484` contains MSP3, App authorization, NVM/boot ordering, PWM3 adapter and tests; `99f6e7a` contains the independent unknown-value UI correction and widget tests; `9af4db2` contains only the bilingual board-validation record. The SDK parent pins Framework `9af4db2` through `37dc8839`. The commits are already grouped by function; no mixed-purpose squash is needed.

### Board validation: retained settings and real backlight

Candidate `demo-settings-20260929` built from Framework `99f6e7a` and SDK `6ec2d080`; OS image SHA256 `cb7cd580be8fb8d6eb14f3e687f1ee579ac01402fe9dcc4a023eb8a4889307a1`, 1,110,016 bytes; OTA package SHA256 `37ea0ee8466cfbd44712d252f35c4955a0b53053dc2e48bc28ffdf9bfc8d552f`, 1,111,040 bytes. Host package integrity and Debug/Release tests passed. COM11/PCAN_USBBUS1 transfer, activation and reboot completed; post-reboot identity is `demo-settings-20260929`, `reference-demo/reference-board`, state `READY`, NVM `ram_revision=10 durable_revision=10 dirty=0`, configured CAN rate index 2 and opened `can0 bitrate=500000`. Existing board settings were explicitly initialized because MSP3 intentionally has no old-record migration. The board adapter reports repeated native CAN transmit refusal while no vehicle sender/termination is connected; this is recorded as a physical bus condition, not a bitrate/NVM failure. UART, OTA events and storage/diagnostic logs are under `evidence/ota/demo-settings-20260929/`. SDL unknown-state captures are under `evidence/adaptation/ui-board-settings/`; both languages show `--` without a duplicate unknown-state label.

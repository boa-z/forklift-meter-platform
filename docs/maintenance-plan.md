# Active maintenance plan

> [中文版](maintenance-plan.zh-CN.md)

Established 2026-09-28 from the [source assessment](maintainability.md), baseline `81e5083`. This is the active continuation record. Update status and evidence here in the same change as the work; do not depend on chat history.

## Behavior-preserving hardening

| Order / item | Scope and rationale | Exit evidence | Status |
|---|---|---|---|
| H-01 | Record the architecture and discrepancies; link this plan from AGENTS and the docs index. Correct M-03, M-11 and verified owner comments to match current code. | Bilingual gate, source review, no changed policy. | Complete |
| H-02 | Make `runtime/meter_periodic.c` locally readable: descriptive parameters, expanded branches, explicit phase comments. Add independent tests for publication rejection atomicity, encoder rejection/identity and exhausted identities. | Five new named cases pass on the original implementation; existing lifecycle assertions retained; full Product matrix passes; optimized objects identical. No signature, layout, arithmetic, deadline or policy changes. | Complete |
| H-03 | Establish negative fixtures for architecture/ownership guards (M-07), then tighten relative-include handling. Keep every previous gate and scope. | Fixtures reject deliberate violations, permit valid includes and fail clearly on missing scan anchors; CTest/CI registers them. Record gate changes under governance rules. | Complete |
| H-04 | Ensure C test setup/checks cannot disappear under NDEBUG (M-08). | Debug and Release both reject a deliberate failing test; test-only enforcement; all Product matrices pass. | Complete |
| H-05 | Document native shared-state/lock ownership and add incremental startup/stop fault-injection seams (M-01/M-05). No worker movement or synchronization change. | Tests cover every initialized resource and acknowledgement, queue rejection and blocked durability; distinguish deterministic stubs from target results. | Queued |
| H-06 | Inventory analyzer coverage and extend handwritten first-party scopes in reviewable groups (M-06); exercise documentation links and generator failure paths (M-09). | Scope artifact reconciles selected sources and compile commands; analyzer findings repaired or explicitly pending human review. | Queued |

For each batch: read affected contracts, characterize before editing, preserve external semantics, run directly relevant tests and the selected Product matrix, and record limitations. Avoid broad formatting churn or new abstraction layers.

## Architecture decisions

| Item | Proposed next action | Authority / state |
|---|---|---|
| D-01 rejection policy | Review actual counters and decide whether semantic validation rejects affect mode. Until decided, document current behavior. | Product/maintainer; open |
| D-02 firmware Product selection | Design one explicit composition boundary for Product storage/UI/protocol sources; assess two target Products before implementation. | Integrator/maintainer; open |
| D-03 lifecycle failures | Decide cleanup/restart/durability failure semantics before changing ownership or shutdown guarantees. H-05 may characterize current behavior independently. | Maintainer; open |
| Governance enforcement | Review existing TAD-001 and configure/verify required checks as described in compliance status. No agent approval or blanket exception. | Human reviewer/repository administrator; pending |

These items are proposals, not accepted architecture decisions. Record rationale, alternatives, compatibility and verification when decided. No CAN/protocol behavior, timing guarantee, thread ownership, storage format, update trust/recovery or diagnostic/safety policy changes are authorized by this plan.

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

Objects share SHA256 `9bf4e998b6baedb2748863bf7f5eeb668dbf059852ad716d8341c0d09f83cfc9`. This is evidence for this host compiler, not a target timing measurement. Raw build/configure logs and a result manifest remain in the ignored `build-maintainer-audit` directory; the table records the durable repository summary. No current-tree firmware/HIL, Linux analyzer or sanitizer acceptance is implied. Existing [source-bound hardware evidence](validation.md) retains its original identity.

## Guard hardening verification

H-03/H-04 are complete on the continuation branch after audit checkpoint `c3ddb09`. Seven real-CLI fixture methods cover allowed/forbidden includes and ownership scan anchors. Debug and Release assertion witnesses execute their deliberate failure; production targets keep their build-type flags. Governance change GCR-002 is recorded for human review in compliance status. Full framework continuation evidence follows in the next batch.

# Engineering governance baseline

## Status and enforcement

Baseline ac3dc01, inspected 2026-09-28. Three independent levels: Tool Quality Green, Project Governance Conforming, Formal MISRA Compliance. The first two are the target. This is a MISRA C:2025-ready first-party engineering process, not formal compliance. No licensed rule text or formal analyzer is available; do not reproduce proprietary rules.

Tool Quality Green: baseline GitHub run 36419980592 attempt 2 passed host and quality. Dynamic TX firmware source 3c141ff passed run 36430288416; Host-tool source 4793bb4 passed run 36430815503. Subsequent code needs a new run. Project Governance Conforming: pending human applicability review and required-check enforcement. Formal MISRA Compliance: not assessed.

Actual main protection inspection returned HTTP 404 (Branch not protected); repository rulesets returned an empty list. These CI jobs are not currently protected required checks. An administrator must configure enforcement before claiming it exists.

## Scope and real analyzer coverage

First-party production scope includes contracts, core, runtime, storage, update, protocols/common, autonomous platform/rtthread and Product C. Generated DBC/catalog C remains production code; retain generator version, inputs and output evidence. tools/analyze_generated.py explicitly checks all three public Products' generated CAN C and the generated Demo catalog with Cppcheck and clang-tidy, using each Product include root and C11. This is portable analysis, not target analysis. Fonts/binary resources are separate assets.

The original Cppcheck and clang-tidy steps enumerate core, runtime, storage and update; the additional generated-production step is listed above. The header filter additionally includes contracts. Host CMake commands do not establish target coverage. Handwritten Product, platform and UI translation units are not comprehensively analyzed by these jobs. Compiler, generator, architecture and public-clean checks supply different evidence, not complete MISRA coverage.

Adopted RT-Thread/ArtInChip SDK, LVGL, CANopenNode and iso14229 retain upstream ownership. Dependency/build evidence must record pinned SHA, license, usage boundary, known findings and validation. Missing evidence stays open; do not modify upstream solely to make first-party tools green.

## Project rules

Every rule requires affected-scope manual review and exact-scope exception records. Automated checks are partial evidence.

| Rule | Intent / scope | Automated enforcement | Manual review |
|---|---|---|---|
| FMP-C-001 | One mutable owner | Ownership gate | State, IPC copies, lifecycle |
| FMP-C-002 | No volatile synchronization or mutable cross-owner borrowing | Architecture/compiler | Lock and lifetime proof |
| FMP-C-003 | Validate external pointers, ranges, invalid states | Analyzer/sanitizer/negative tests | External boundary audit |
| FMP-C-004 | Bounded resources and runtime heap | Architecture gate | Capacity, ISR bounds, blocking, stack |
| FMP-C-005 | Handle meaningful failures/results | Compiler/analyzer/fault tests | Admission versus completion |
| FMP-C-006 | Bounded memory/string operations | Analyzer/sanitizer | Length and overlap proof |
| FMP-C-007 | Portable core/runtime, owner-only I/O | Architecture/ownership | No OS/LVGL/device escape |
| FMP-C-008 | Monotonic wrap-safe time | Deadline tests | Half-range and expiry proof |

## Exceptions and TAD-001

Record ID, rule/tool, exact location, rationale, risk, alternatives, verification, owner, approval status and review condition. Try code repair first. Agents cannot approve exceptions. Gate configuration changes require an explicit record. Preserve warnings, analyzer scope, sanitizers, fuzz bounds and HIL thresholds; never delete tests, weaken errors, exclude first-party files or misclassify code to pass.

TAD-001 is a Tool Applicability Decision, not a MISRA deviation. Scope: the existing WarningsAsErrors exclusion for clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling in .clang-tidy. Checker warnings stay enabled/visible. Owner: project maintainer. Approval: PENDING human review. Reason: Annex K replacement availability is not established for target libc. Risk: the checker covers other APIs needing per-location review; the broad severity exception does not prove safety. Alternative: audited libc support or narrower reviewed locations. Evidence: bounded copy/null/geometry fixes in 4e31062/ac3dc01 and the baseline quality run. Review on libc, checker, toolchain or call-site changes. No additional suppression is introduced here.

## Toolchain evidence

Capture actual target GCC/Xuantie, host GCC, Clang, clang-tidy, Cppcheck and Python versions alongside each build. ASan, UBSan and libFuzzer are compiler runtime capabilities, not licensed MISRA analyzers. Record actual SCons target definitions, includes, march, mabi, optimization and language mode; a host compilation database is insufficient. Version drift or missing target analysis must remain visible in the delivery report.

## Maintenance guard change record

GCR-002 (2026-09-28): strengthen only `tools/check_architecture.py`, `tools/check_runtime_ownership.py` and CMake test targets. Relative quoted includes now resolve from the source directory before known roots; Demo protocol includes resolve against its Product root and canonical allowed paths. Ownership scanning rejects absent, duplicate or reversed anchors. Existing forbidden calls, scanned areas and dependency rules remain. Disposable negative fixtures run the real CLI entry points through CTest and pytest. These lexical checks do not prove transitive call ownership, macro-expanded includes or race freedom.

C test targets named `test-*`/`test_*` and the existing SDO test peer explicitly undefine NDEBUG; production targets retain build-type flags. A compile-time guard plus a failing assertion-expression witness checks enforcement in Debug and Release. This adds checks, not exemptions, and changes no production contract. Owner: maintainer; implementation evidence is in the maintenance plan; human review pending. Review on compiler, test naming, include-root or scanned function layout changes. No TAD approval, suppression or reduced HIL threshold is introduced.

GCR-003 (2026-09-28): add Release/headless + update CTest to the host workflow and native lifecycle fault-injection tests. Preserve existing Debug, sanitizer, fuzz, generated-source and hardware gates. The explicit handwritten analyzer extension will retain existing scopes and identify native stub assumptions in its evidence. Owner: maintainer; human review pending. Review on owner layout, resource initialization order, toolchain or target include configuration changes. No suppression or safety-policy approval is granted.

The implemented extension adds `tools/analyze_handwritten.py` and broadens Clang header reporting to diagnostics/platform/protocols without removing existing scopes. Seven explicit sources run through both analyzers in CI; native execution uses identified host stubs with OTA disabled, never target provenance. Scope evidence includes source/configuration hashes and tool commands/versions. Generic firmware Product includes and reference UI imports of runtime internals now have negative architecture/ownership fixtures. The Shell stub preserves its exported function reference so host analysis sees the same ownership use; no first-party warning was suppressed to bypass an unused-function finding.


GCR-004 (2026-09-29): add headless Demo presentation, runtime calibration and diagnostic classification to explicit handwritten analysis; retain previous scopes. Product renderer includes exclude runtime/transport internals; negative fixtures reject Domain interpretation in Demo screens and rendering/OS types in projection. Lexical checks supplement review. No suppression, HIL threshold change or health-policy approval. Owner: maintainer; human review pending; review on Product include layout or analyzer context changes.

GCR-005 (2026-09-29): extend the reference UI lexical guard to reject remote key/work/reply/result types; negative fixtures exercise all four forms. Existing scopes and thresholds remain. This enforces the example boundary, not macro-expanded or transitive type analysis. Maintainer review pending; no suppression or production policy approval.

GCR-006 (2026-09-29): enlarge Demo UI typography and spacing within the existing 800x480 canvas, using only pinned LVGL faces and the checked-in Chinese subset. Preserve the four-page Product composition, presentation boundary, translation tags and widget semantics. Review on font generation, screen geometry or visual acceptance changes; no customer assets or private Product rules are imported.

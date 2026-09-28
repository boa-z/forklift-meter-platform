# Runtime, storage and update delivery

## Baseline and authority

The September 28, 2026 user-approved development requirements and governance addendum govern this work. Their SHA256 identities are respectively B546D42269F26DD736A5DD0A08481EAF0B36B53C68009D8C3CE72BE133E46C21 and B072FBE8E1F573A3CD7260EEB3A64DE4C3FCD5C95E669A2F9A60F51590E3F240. Local original copies and source evidence live in ignored evidence/framework-baseline. They are specifications, not acceptance evidence.

Start from Platform ec3f087cd8406fb2d374828e28c5fd3a7117f2f6 and SDK 7608a666f047783c813438d40494c65479e77788. The older beb680e baseline has the same Platform tree after history consolidation. Development uses codex/runtime-nvm-ota; main requires independent review. The installed firmware remains the previously reported 5aa2a205 build until an identified replacement is actually flashed. Existing six-test Demo HIL remains five PASS and Burst FAIL; Mixed HIL is NOT_RUN.

## Decisions and gates

| Decision | Source and disposition | Remaining gate |
|---|---|---|
| D-01 build | SDK config: 1 MiB SRAM, 16 MiB PSRAM, 1000 Hz tick, 32 priorities; source hashes retained locally | Capture actual target commands, ELF/MAP, compiler model and measured budgets |
| D-02 EEPROM | Board header names BL24C512A, i2c0, address 0x50, 64 KiB, 128-byte pages | Confirm actual part, WP, timing and permission for a new layout before writes |
| D-03 layout | SDK pack declares 4 MiB os/os_r, 14 MiB rodata/rodata_r, redundant ENV, separate data/data_r | Verify running layout, image growth and recoverable first installation; no flash writes yet |
| D-04 transport | Updated task selects UDS + ISO-TP; review pinned iso14229 and retain SDK OTA/ENV | Complete protocol/backend audit and board loop |
| D-05 trust | Product/Hardware, length and complete SHA256; hash is not a signature | Signing, keys and anti-rollback belong to future Security Framework |
| D-06 timing | Synthetic 0x3C0/50 ms and 0x2F0/100 ms; record distributions | Product jitter, maintenance and admission policy remain unconfirmed |
| D-07 records | Demo preferences and explicitly local synthetic parameters share MSP2; other Products opt in | Remote parameters must not implicitly acquire local authority |
| D-08 trial | Current SDK mount calls early status confirmation; activation ignores flush result | Separate finalize/activate/health-confirm and prove recovery before OTA HIL |

The CAN OTA task supersedes the earlier SDO transport proposal. SDK UDS request validation, blocking callback writes and block counters require replacement. A mature UDS/ISO-TP library owns transport; an independent Update worker reuses the ArtInChip installer. Activation waits for the NVM durable barrier; transfer completion is not installation or upgrade success.

## Ownership and execution

App alone changes Domain and Product workflows. Protocol decodes complete per-frame semantic batches; a full batch must fit before it can be delivered. UI owns LVGL and consumes copied arrays. App working, published, and UI-local storage are distinct and Product-sized. Snapshot copying is synchronous and bounded under the platform publication mutex, with capacity and alias checks before any mutation. It does not provide its own lock.

Protocol owns protocol stacks and TX tickets. A per-bus TX worker alone calls SDK write. NVM and Update workers own their backend state and blocking I/O. Every accepted request reserves retained result capacity until acknowledgement. QUEUED, APPLIED, DURABLE, DRIVER_COMPLETED and remote response are separate meanings. Session/generation identifies stale local completions; it cannot identify every late on-wire SDO reply.

RT-Thread native MQ/event/mutex connects these owners. Pure C step functions have one owner and no implicit thread safety. Worker messages own values or bounded pool handles; they never borrow producer stacks. In-driver or in-I/O requests cannot be reclaimed on a local timeout. Stop failure preserves resources.

## Work packages and acceptance

| Package | Deliverable | Status at opening |
|---|---|---|
| W0/G0 | Frozen evidence and finite decisions; source classification and enforcement plan | IN_PROGRESS; hardware and licensed standard/tool inputs pending |
| W1 | Deep-copy, complete batch, typed request/result and service contracts with Host tests | IN_PROGRESS |
| W2 | Protocol/App/UI, TX workers, periodic TX, asynchronous SDO, safe diagnostics | NOT_IMPLEMENTED; original Burst remains FAIL |
| W3/G1 | Record codec, revision/barrier, slots, EEPROM/File backends and independent worker | HOST_PASS / BUILD_PASS; BOARD_NOT_RUN / POWER_CUT_NOT_RUN |
| W4 | UDS + ISO-TP transfer, bounded blocks, manifest and Host downloader | NOT_IMPLEMENTED |
| W5 | SDK EEPROM/OTA/ENV fixes, candidate and trial confirmation | NOT_IMPLEMENTED |
| W6/G4 | Joint firmware, original HIL plus storage/update recovery, compliance closure | NOT_RUN |

Host, BUILD, BOARD and POWER_CUT results are separate per backend. A source checkpoint does not mean SOFTWARE_COMPLETE or COMPLETE_ACCEPTED. No physical writes or power interruption follow merely from Host tests. Preserve original HIL inputs and threshold; do not hide native CAN FIFO drops.

## Governance

First-party C, including AI-produced C, is native code. Deterministically generated code retains input/tool identities and participates in analysis. RT-Thread, LVGL, CANopenNode and SDK are adopted components requiring version, license and boundary evidence. Own lvgl-aic logic is not automatically third-party. Host-only tools have a separate quality scope.

The user confirmed no licensed standard or formal checker is available. Continue under project engineering rules: single ownership, explicit errors, bounds checks, Chinese comments and existing CI. No formal MISRA compliance certification is claimed; adopted boundaries and unverified hardware capabilities remain explicit.

## Evidence and next gate

Per-commit reports identify actual source, tests, limitations and next package. Evidence is stored locally under evidence/ and excluded from the public source tree. Physical artifacts must bind the flashed image SHA256 and actual running identity. Unknown part geometry, key deployment, recovery or licensing cannot be replaced by illustrative constants.

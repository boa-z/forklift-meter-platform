# CAN OTA technical reference

> [中文版](can-update-internals.zh-CN.md)

For package creation and installation, start with the [operator guide](can-update.md). This page preserves implementation constraints and historical investigation; it is not the operating procedure.

## Design decision

CAN OTA is an optional Framework capability. The intended path is CAN → ISO-TP/UDS → copied bounded job → single Update worker → existing ArtInChip OTA installer → inactive A/B candidate → NVM durable barrier → activation → reboot → new-firmware observation.

Product owns admission and maintenance policy. Core and UI do not parse CAN or call Flash.

No complete firmware is cached in RAM and no custom CAN file-fragment protocol is introduced.

The optional Update worker reuses the common production Protocol and CAN TX owners and unchanged native ArtInChip installer. Transfer completion, candidate verification, activation and new-firmware observation are distinct.

Native SDK confirmation clears its upgrade flag before application health checks: native_auto is not application-controlled confirmation. SHA256 is integrity checking, not a signature; rollback and download-time power loss remain unverified.

## SDK review

Audit baseline: SDK 9b78386dcaa54326487c8b208b2b7104d4b56c9b, tracked tree equal to 7608a666. No SDK source/configuration or parent gitlink changes are made.

| Area | Observed implementation | Consequence |
|---|---|---|
| packages/artinchip/ota/ota.h | Exposes ota_init, ota_deinit, ota_shard_download_fun | No candidate policy or complete-package query API |
| packages/artinchip/ota/ota.c | Package metadata selects partitions; a file header can trigger erase before the complete package arrives | Require an application-side constrained package policy before forwarding bytes; never rely only on host checks |
| packages/artinchip/ota/ota.c | Checks per-file additive sums and readback sums | Does not replace complete package SHA256 or strict trailer/length validation |
| packages/artinchip/env/absystem_os.c | aic_upgrade_end and aic_ota_status_update do not propagate fw_env_flush return | Return code alone is insufficient proof of durable activation/confirmation |
| packages/artinchip/env/absystem_os.c | Mount path calls aic_ota_status_update before application health/NVM validation | Cannot advertise application-controlled trial confirmation on this baseline |
| packages/artinchip/uds | Existing example download handlers use synchronous file I/O | Do not route those callbacks into the production Flash worker contract unchanged |
| Current composition | SDK OTA downloader disabled; optional application composition links unchanged ota.c/burn.c | New endpoint exists only with METER_CAN_UPDATE=1 |

The default unsupported backend remains a Host boundary test. METER_CAN_UPDATE=1 selects meter_update_backend_aic.c: it validates physical redundant ENV CRC before opening the native ENV API, rejects pending/inconsistent slots and overlapping partitions, and requires the audited 2048-byte page / 64-page block / 32-block inactive OS geometry.

Only an inactive partition without bad blocks is accepted. Native Flash erase/write/read and A/B selection remain vendor-owned.

The missing optional NFTL initialization symbol has an application-side rejecting implementation only when NFTL is disabled; no data partition is admitted.

A complete metadata and OS header prefix is buffered and checked before native erase. Then only complete 4096-byte OS blocks are forwarded.

Full package/trailer/length/SHA256 validation and SHA256 readback of the installed OS must succeed before activation. The NVM durable barrier precedes native activation; ENV is closed, reread and checked after activation because the native API ignores flush failure.

Errors never trigger an automatic reboot. This constrained integration does not establish application-health confirmation or rollback.

## Dependencies and resource bounds

The firmware protocol reuses iso14229 release 0.11.0 (commit 8a7eb23d6ab9d52f66ce36b07ba456613709c78e) and its bundled isotp-c transport, both MIT licensed; upstream notices stay in the pinned submodule. No upstream business logic is modified.

Host dependencies are pinned in tools/ota/requirements.txt: python-can 4.6.1 (LGPL-3.0), can-isotp 2.0.7 (MIT), udsoncan 1.25.1 (MIT). These are host dependencies, not firmware libraries.

UDS receive/send buffers are 1024 bytes each; each copied update block is at most 512 bytes. The wire contract for every identifier is defined once, in protocols/uds/meter_uds_did.h, and is documented in [UDS DID reference](#uds-did-reference).

Host preflight reads at most 64 KiB at a time; transfer blocks are at most 512 bytes. The same open file is rehashed during transfer and changed content aborts before verification.

The CLI respects the device block limit and receiver STmin/flow control. The exclusive-maintenance profile requests zero STmin and eight-frame flow-control blocks, and removes the former 20000-bit/s Host data limiter.

It never overrides a slower receiver STmin. Firmware uses the upstream configurable one-millisecond response scheduling delay; pending operations retain the original bounded timeout.

Normal/Burst HIL thresholds outside maintenance remain unchanged.

## UDS DID reference

protocols/uds/meter_uds_did.h is the only definition of this contract; the tables below must match it. 0xF180 (bootSoftwareIdentification) is removed and must not be reintroduced. 0xFD00-0xFEFF is the range ISO 14229-1 reserves for system suppliers, so the update identifiers live there; F186/F192/F195 are the standard supplier identifiers.

| DID | Name | Length | Services | Restriction |
|---|---|---|---|---|
| 0xFD10 | update manifest | 148 | 0x22 read, 0x2E write | 0x2E only in the programming session |
| 0xFD11 | update status | 12 | 0x22 read | none |
| 0xFD12 | update endpoint | 164 | 0x22 read | none |
| 0xF186 | active diagnostic session | 1 | 0x22 read | none |
| 0xF192 | hardware number | 32 | 0x22 read | none |
| 0xF195 | software version | 48 | 0x22 read | none |

One 0x22 may carry several identifiers; the response concatenates them in request order. Every identifier is fixed length, and a Host that expects another length is a protocol error rather than a longer or shorter value. CAN never carries JSON: the UART `meter_update info` command still emits JSON, but it is built from the same device snapshot the identifiers encode.

### FD10 update manifest (148 bytes)

| Offset | Length | Field |
|---|---|---|
| 0 | 32 | Product id |
| 32 | 32 | Hardware id |
| 64 | 48 | Package version |
| 112 | 4 | Package length, big-endian uint32, non-zero |
| 116 | 32 | SHA256 of the complete package |

Text fields are ASCII, NUL terminated and zero padded to the field width; the allowed characters are A-Z a-z 0-9 . _ + -. A field with no NUL inside its width, a non-ASCII byte, an empty field or a zero length rejects the write. Strings are never truncated: an over-long Product identity fails at start-up instead. Reading FD10 while no manifest is authoritative returns 148 zero bytes.

### FD11 update status (12 bytes)

| Offset | Length | Field |
|---|---|---|
| 0 | 1 | Update state |
| 1 | 1 | Last error |
| 2 | 1 | Flags |
| 3 | 1 | Reserved, zero |
| 4 | 4 | Received bytes, big-endian |
| 8 | 4 | Total bytes, big-endian |

States: 0 idle, 1 downloading, 2 transferred, 3 verifying, 4 candidate verified, 5 waiting for the durable barrier, 6 activating, 7 activated, 8 confirmed, 9 aborted, 10 failed. Errors: 0 none, 1 state, 2 compatibility, 3 denied, 4 length, 5 order, 6 backend, 7 hash, 8 timeout, 9 NVM, 10 session, 11 format, 12 unsupported. Both numbering tables use designated initializers, so reordering a C enum cannot change a wire byte.

Flags: bit0 backend supported, bit1 maintenance active, bit2 exclusive maintenance, bit3 admission passed, bit4 manifest present, bit5 candidate verified. Bits 6-7 are reserved and always zero.

Progress is defined per state: idle, aborted and confirmed report 0/0; downloading reports the live values; transferred, verifying, candidate, waiting, activating and activated report total/total; failed keeps the values at the moment of failure for diagnosis. Total is never inferred from "a manifest exists".

### FD12 update endpoint (164 bytes)

| Offset | Length | Field |
|---|---|---|
| 0 | 32 | Product id |
| 32 | 32 | Hardware id |
| 64 | 48 | Running firmware version |
| 112 | 16 | OS member name inside the package |
| 128 | 4 | Inactive slot capacity, big-endian |
| 132 | 32 | Admission rejection reason |

Text uses the same encoding as FD10. The OS member name and the rejection reason are sized to these fixed fields; no longer diagnostic string exists, so the native backend reports env_or_slot_invalid or native_auto_unverified and the rejecting backend reports sdk_validation_confirm_unavail. An empty reason means no rejection.

### F186, F192, F195

F186 returns the one-byte session type. F192 returns the 32-byte hardware identity and F195 the 48-byte OTA firmware version taken from METER_UPDATE_FIRMWARE_VERSION. The display version METER_FIRMWARE_DISPLAY_VERSION is UI text only and is never exposed through F195.

### Services, permissions and NRC

0x2E FD10 checks only the identifier, the programming session, the 148-byte length and the field syntax: a different or non-writable identifier is 0x31, a non-programming session is 0x7F, a wrong length is 0x13, malformed text or a zero package length is 0x31, and an existing candidate, an active transfer or an unfinished request is 0x22. It does not check Product/Hardware match, capacity or admission, and it never touches Flash.

0x34 owns admission and the transfer itself: without a staged manifest it is 0x22; a non-zero address, a non-zero data format identifier or a length that differs from the manifest is 0x31; Product/Hardware mismatch, capacity and admission failures come back through the worker error mapping. The two services therefore do not share one NRC table by design.

Manifest lifecycle:

- 0x2E FD10 stores a staged manifest that FD10 can read back before any download starts.
- 0x34 moves authority to the worker. When the asynchronous Begin finally completes, successfully or not, the staged copy is dropped, so a later failed or aborted state cannot expose the previous request's manifest. A pending 0x78 does not drop it early.
- The worker manifest is valid only while downloading, transferred, verifying, candidate, waiting for the durable barrier, activating or activated. Failed and aborted read as 148 zero bytes.
- After a failure a new 0x2E FD10 succeeds, and FD10 becomes readable again even before a new download begins.
- Session timeout and 0x10 01 drop the staged manifest and any incomplete transfer but keep a verified candidate, which the worker still owns.

Normal sequence:

```text
0x10 02                     enter the programming session
0x22 FD12 FD11              optional preflight: endpoint and status
0x2E FD10 <148 bytes>       submit the manifest
0x34                        address zero, the declared length
0x36 <1..512 bytes> x N     transfer, standard modulo-256 counter
0x37                        device verification
0x31 01 F001                activate
0x11 01                     reset, then observe the new version through F195
```

0x31 01 F002 aborts and discards a candidate; 0x31 01 F003 is reserved for device confirmation. ECUReset is accepted only after activation, and the CLI never treats sending a reset as new-firmware confirmation.

## OS-only package policy

The shared C validator in update/meter_package.c accepts aic-os-only-crc-cpio-v1. Host inspection and the board worker use the same implementation.

Its context remains below 1 KiB, with no heap and arbitrary input boundaries. The adapter separately maintains streaming package/image SHA256; it never caches a complete firmware image.

Only CRC CPIO (070702) with exactly ota_info.bin, the Product-selected OS .itb member and TRAILER!!! in that order is accepted. Metadata must be 512 bytes, declare the exact package length and target version, and map only ota_info.bin:file and the OS member:os.

Names, regular-file type, single link, member checksums, zero alignment padding and final 512-byte archive alignment are checked. Extra partitions, duplicate members, traversal, alternate target mappings and truncated archives are rejected.

The OS must fit the actual inactive capacity rounded down to the vendor 4096-byte write granularity. The policy capacity comes from the integrator/device, never from the archive.

Native metadata limits the version to 31 ASCII characters; the general UDS manifest remains 48 bytes wide.

The four-byte package metadata prefix is covered by CPIO checksum and outer SHA256. It is distinct from the actual boot ENV: physical ENV CRC is independently checked before native slot selection.

FIT header and declared FIT size are checked before erase; full OS SHA256 is checked after native readback. These checks do not authenticate the embedded Product or prove bootability.

The bootloader remains the authority for loading FIT.

The SDK mkcpio.py changes archive metadata after creation without rebuilding the CPIO checksum and edits its input configuration. Framework pack instead invokes unchanged mkenvimage and GNU cpio twice in a temporary directory, validates the result and publishes ota.cpio, ota.manifest.json and package-report.json. --pad-os-to 4096 pads only the temporary OS copy with 0xFF; padding is included in CPIO checksums and package SHA256.

Use this option for the board adapter. The manifest is operator-supplied Product/Hardware metadata, not firmware identity authentication.

Linux CI installs cpio and u-boot-tools (mkenvimage); Windows can pass the SDK tool executable paths explicitly. Tests use real native tools plus public synthetic payloads; synthetic .itb data is not a bootable image.

## Validation and remaining work

Before connecting, read `meter can 0` over UART and pass its reported bitrate explicitly with `--bitrate`. The CLI default is not the Product default; a mismatch can cause bus-off even with correct wiring.

A discovered PCAN channel does not prove which board CAN connector is attached.

The activation barrier follows the Product storage contract: absent or explicitly disabled storage has no settings revision to persist. Enabled storage still requires a nonzero requested revision and a successful durable check; unavailable or failed enabled storage never qualifies for the no-storage path.

Host regression covers absent/disabled storage, missing revision, pending persistence and durable completion.

Additional tests cover chunk boundaries, CPIO/metadata faults, aligned capacity, native packaging and no publication after a tool failure. Host tests also exercise manifest bounds, corruption, identity/maintenance/capability rejection before mutation, block counter wrap, changing files, timeout/Abort and activation target checks.

A python-can virtual bus test exchanges an actual segmented ISO-TP response using can-isotp and udsoncan and parses the resulting CAN evidence. This is a host transport test, not PCAN hardware or an OTA installation.

C tests cover state transitions, worker completion, malformed UDS requests and the real rejecting backend without SDK linkage. They also pin the DID contract: identifier lengths, NRC per service, the per-state progress semantics, manifest lifecycle across Begin, session timeout and failure, and the refusal to start with an over-long identity. Linux CI adds the optional update configuration; local tests do not mean remote CI has run.

Native-parser/crypto Host regression covers arbitrary chunks, invalid ENV, unsafe geometry, malformed metadata/FIT/trailer, bad blocks, erase/write/read failures, readback corruption, hash mismatch, Abort and lost ENV flush. Flash/ENV in those tests are doubles.

Host failure tests do not establish physical power-loss or rollback behavior.

## Board test build

Build through SDK OneStep with `METER_CAN_UPDATE=1` and `METER_UPDATE_VERSION`, as described in the [build guide](../build/build.md). The build writes the OTA package to `output/<project>/ota/<version>/` after it succeeds.

OTA is enabled only through application build settings. Vendor OTA objects go into `output/<project>/meter/sdk-ota`.

SDK source/configuration/gitlinks are not edited. Build baseline ota-board-a and candidate ota-board-b from the same committed source; flash only the baseline image initially.

The candidate OS becomes a CAN package. The reported version field must change after reboot.

Thread ownership, stacks and CAN queue budgets are maintained in [production runtime](../runtime/runtime-production.md). Update retains one job and one completion credit; cancellation prevents new work until old jobs/results are drained.

Only CAN TX workers block in driver writes; App owns Domain and UI owns LVGL. The native backend has a 1536-byte prefix buffer, 4096-byte write/readback buffers; vendor OTA uses two 8192-byte buffers and a 4096-byte readback buffer.

These are fixed capacities, not measured worst-case stacks.

The synthetic Demo requires local maintenance and ready NVM; it has no vehicle safety interlocks. Maintenance freezes settings writes; activation waits up to five seconds for the requested revision to become durable.

Timeout cancels incomplete transfers, while a verified candidate remains available for a later CLI activation. Abort discards a candidate but cannot undo activation.

Product mode policy permits critical periodic TX while suppressing ordinary telemetry/TX, using generation to invalidate old work. The bilingual update overlay replaces live readings; percentage represents received bytes only.

Diagnostics, ISO-TP/UDS, Update and the NVM barrier keep running. App alone controls maintenance entry/exit.

External cyclic senders may stop for a dedicated speed run; coexistence and normal HIL retain their original thresholds.

After flashing, first verify UI, touch, meter info, NVM READY and backend_supported=true without enabling maintenance. Then enable maintenance for controlled testing.

Preserve UART startup including native slot selection, PCAN ASC, package hash, before/after firmware version and update diagnostics. Wrong Product, damaged package, interruption, explicit Abort, maintenance entry/exit, UART responsiveness and post-update normal CAN recovery require separate evidence.

A human-controlled power cut is a separate test; neither this build nor Host PASS establishes its result.

## Hardware transport observations

The first board session exposed two integration issues: the synchronous UDS client waits for a complete ISO-TP response, while the negotiated 150 ms P2 budget is shorter than a segmented diagnostic JSON response at 5 ms STmin; the RT-Thread console also truncates a single long formatted write. The Host connection now uses an explicit bounded two-second complete-response budget and preserves the 120-second response-pending limit.

This is a client assembly budget, not a claim about ECU P2 compliance. Serial diagnostics emit bounded chunks.

A regression performs session negotiation before receiving a long real ISO-TP response, and the adapter test uses a 128-byte console buffer.

CAN evidence preserves native backend RX timestamps; Host TX observations may have a different clock origin. Use ordered log windows and frame identities for correlation, and calculate RX intervals within one native clock.

Do not subtract native uptime timestamps from Host epoch time. Periodic background traffic shares one python-can owner and stops before channel shutdown.

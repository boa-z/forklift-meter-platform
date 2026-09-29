# CAN firmware update: application integration and board validation

## Design decision

CAN OTA is an optional Framework capability. The intended path is CAN → ISO-TP/UDS → copied bounded job → single Update worker → existing ArtInChip OTA installer → inactive A/B candidate → NVM durable barrier → activation → reboot → new-firmware observation. Product owns admission and maintenance policy. Core and UI do not parse CAN or call Flash. No complete firmware is cached in RAM and no custom CAN file-fragment protocol is introduced.

The optional Update worker reuses the common production Protocol and CAN TX owners and unchanged native ArtInChip installer. Transfer completion, candidate verification, activation and new-firmware observation are distinct. Native SDK confirmation clears its upgrade flag before application health checks: native_auto is not application-controlled confirmation. SHA256 is integrity checking, not a signature; rollback and download-time power loss remain unverified.

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

The default unsupported backend remains a Host boundary test. METER_CAN_UPDATE=1 selects meter_update_backend_aic.c: it validates physical redundant ENV CRC before opening the native ENV API, rejects pending/inconsistent slots and overlapping partitions, and requires the audited 2048-byte page / 64-page block / 32-block inactive OS geometry. Only an inactive partition without bad blocks is accepted. Native Flash erase/write/read and A/B selection remain vendor-owned. The missing optional NFTL initialization symbol has an application-side rejecting implementation only when NFTL is disabled; no data partition is admitted.

A complete metadata and OS header prefix is buffered and checked before native erase. Then only complete 4096-byte OS blocks are forwarded. Full package/trailer/length/SHA256 validation and SHA256 readback of the installed OS must succeed before activation. The NVM durable barrier precedes native activation; ENV is closed, reread and checked after activation because the native API ignores flush failure. Errors never trigger an automatic reboot. This constrained integration does not establish application-health confirmation or rollback.

## Dependencies and resource bounds

The firmware protocol reuses iso14229 at commit 2e36afcd7f0cd02b0c70446c1a265a7b1999d478 and its bundled isotp-c transport, both MIT licensed; upstream notices stay in the pinned submodule. No upstream business logic is modified. Host dependencies are pinned in tools/ota/requirements.txt: python-can 4.6.1 (LGPL-3.0), can-isotp 2.0.7 (MIT), udsoncan 1.25.1 (MIT). These are host dependencies, not firmware libraries.

UDS receive/send buffers are 1024 bytes each; each copied update block is at most 512 bytes. The 148-byte manifest is Product[32], Hardware[32], Version[48], uint32 big-endian package length and SHA256[32]. DID 0xF180 writes the manifest and reads JSON diagnostics. RequestDownload uses address zero and the exact declared length. TransferData uses the standard modulo-256 counter; TransferExit requests device verification. StartRoutine 0xF001 activates, 0xF002 aborts, 0xF003 is reserved for device confirmation. ECUReset is permitted only after device activation. CLI never treats sending a reset as new-firmware confirmation.

Host preflight reads at most 64 KiB at a time; transfer blocks are at most 512 bytes. The same open file is rehashed during transfer and changed content aborts before verification. The CLI respects the device block limit and receiver STmin/flow control. The exclusive-maintenance profile requests zero STmin and eight-frame flow-control blocks, and removes the former 20000-bit/s Host data limiter. It never overrides a slower receiver STmin. Firmware uses the upstream configurable one-millisecond response scheduling delay; pending operations retain the original bounded timeout. Normal/Burst HIL thresholds outside maintenance remain unchanged.

## OS-only package policy

The shared C validator in update/meter_package.c accepts aic-os-only-crc-cpio-v1. Host inspection and the board worker use the same implementation. Its context remains below 1 KiB, with no heap and arbitrary input boundaries. The adapter separately maintains streaming package/image SHA256; it never caches a complete firmware image.

Only CRC CPIO (070702) with exactly ota_info.bin, the Product-selected OS .itb member and TRAILER!!! in that order is accepted. Metadata must be 512 bytes, declare the exact package length and target version, and map only ota_info.bin:file and the OS member:os. Names, regular-file type, single link, member checksums, zero alignment padding and final 512-byte archive alignment are checked. Extra partitions, duplicate members, traversal, alternate target mappings and truncated archives are rejected. The OS must fit the actual inactive capacity rounded down to the vendor 4096-byte write granularity. The policy capacity comes from the integrator/device, never from the archive. Native metadata limits the version to 31 ASCII characters; the general UDS manifest remains 48 bytes wide.

The four-byte package metadata prefix is covered by CPIO checksum and outer SHA256. It is distinct from the actual boot ENV: physical ENV CRC is independently checked before native slot selection. FIT header and declared FIT size are checked before erase; full OS SHA256 is checked after native readback. These checks do not authenticate the embedded Product or prove bootability. The bootloader remains the authority for loading FIT.

The SDK mkcpio.py changes archive metadata after creation without rebuilding the CPIO checksum and edits its input configuration. Framework pack instead invokes unchanged mkenvimage and GNU cpio twice in a temporary directory, validates the result and publishes ota.cpio, ota.manifest.json and package-report.json. --pad-os-to 4096 pads only the temporary OS copy with 0xFF; padding is included in CPIO checksums and package SHA256. Use this option for the board adapter. The manifest is operator-supplied Product/Hardware metadata, not firmware identity authentication.

Linux CI installs cpio and u-boot-tools (mkenvimage); Windows can pass the SDK tool executable paths explicitly. Tests use real native tools plus public synthetic payloads; synthetic .itb data is not a bootable image.

## CLI

Run from the Framework root. Replace CAPACITY_BYTES with the actual inactive capacity in decimal bytes; the OS member name must also match Product policy. pack accepts --cpio and --mkenvimage for native tool paths. METER_OTA_INSPECTOR can select the built inspector executable. A manifest JSON must contain product, hardware, version, size and lowercase sha256. It is trusted input from the packaging workflow, not authenticated proof of embedded firmware identity. Preflight checks the supplied bytes against it; archive structure, installed image and compatibility must still be verified on the device. Arbitrary data with a matching hash is not a validated OTA archive.

~~~text
python -m pip install -r tools/ota/requirements.txt
cmake -S . -B build-package -DMETER_BUILD_UI=OFF
cmake --build build-package --target meter-ota-inspect
python -m tools.ota pack firmware.itb ota-output --product reference-demo --hardware reference-board --version v2 --os-file d13x_os.itb --candidate-capacity CAPACITY_BYTES
python -m tools.ota preflight ota.cpio --manifest ota.manifest.json --os-file d13x_os.itb --candidate-capacity CAPACITY_BYTES
python -m tools.ota --evidence evidence/ota-probe probe
python -m tools.ota --evidence evidence/ota-download download ota.cpio --manifest ota.manifest.json --os-file d13x_os.itb --candidate-capacity CAPACITY_BYTES
python -m tools.ota --evidence evidence/ota-activate activate --version v2 --reboot
python -m tools.ota --evidence evidence/ota-abort abort
~~~

PCAN defaults to PCAN_USBBUS1 at 500000 bit/s, request ID 0x7E0 and response ID 0x7E8. Evidence directories must be new. Network operations produce flushed JSONL progress/errors and a CAN ASC trace containing sent and received frames. PCAN-View is not automated. Probe is read-only. Downloads require matching Product/Hardware, advertised backend support, compatible device OS filename/candidate capacity, maintenance admission and an idle/failed/aborted device. Missing capability advertisement is a rejection. Download never activates automatically. Activation requires an explicit matching target version; reboot is a separate flag. Transport/session failures request Abort where possible and preserve the original error if Abort also fails. A disconnected device may not receive Abort and must enforce its own timeout.

## Validation and remaining work

Before connecting, read `meter can 0` over UART and pass its reported bitrate explicitly with `--bitrate`. The CLI default is not the Product default; a mismatch can cause bus-off even with correct wiring. A discovered PCAN channel does not prove which board CAN connector is attached.

The activation barrier follows the Product storage contract: absent or explicitly disabled storage has no settings revision to persist. Enabled storage still requires a nonzero requested revision and a successful durable check; unavailable or failed enabled storage never qualifies for the no-storage path. Host regression covers absent/disabled storage, missing revision, pending persistence and durable completion.

Additional tests cover chunk boundaries, CPIO/metadata faults, aligned capacity, native packaging and no publication after a tool failure. Host tests also exercise manifest bounds, corruption, identity/maintenance/capability rejection before mutation, block counter wrap, changing files, timeout/Abort and activation target checks. A python-can virtual bus test exchanges an actual segmented ISO-TP response using can-isotp and udsoncan and parses the resulting CAN evidence. This is a host transport test, not PCAN hardware or an OTA installation. C tests cover state transitions, worker completion, malformed UDS requests and the real rejecting backend without SDK linkage. Linux CI adds the optional update configuration; local tests do not mean remote CI has run.

Native-parser/crypto Host regression covers arbitrary chunks, invalid ENV, unsafe geometry, malformed metadata/FIT/trailer, bad blocks, erase/write/read failures, readback corruption, hash mismatch, Abort and lost ENV flush. Flash/ENV in those tests are doubles. Current source-bound board throughput, maintenance, NVM and reboot results are in [validation](validation.md); Host failures tests do not establish physical power-loss or rollback behavior.

## Board test build

Use tools/ota/build_board.py with --sdk-root, --version and a new --output directory. Supply --python for the SDK Python executable if required. It enables OTA only through application build arguments, archives the image/ELF/map/OS/ENV and source identities, records the effective config and restores original .config bytes in finally. Vendor objects go into ignored application build-firmware/sdk-ota. SDK source/configuration/gitlinks are not edited. Build baseline ota-board-a and candidate ota-board-b from the same committed source; flash only the baseline image initially. The candidate OS becomes a CAN package. Reported firmware_version must change after reboot.

Thread ownership, stacks and CAN queue budgets are maintained in [production runtime](runtime-production.md). Update retains one job and one completion credit; cancellation prevents new work until old jobs/results are drained. Only CAN TX workers block in driver writes; App owns Domain and UI owns LVGL. The native backend has a 1536-byte prefix buffer, 4096-byte write/readback buffers; vendor OTA uses two 8192-byte buffers and a 4096-byte readback buffer. These are fixed capacities, not measured worst-case stacks.

The synthetic Demo requires local maintenance and ready NVM; it has no vehicle safety interlocks. Maintenance freezes settings writes; activation waits up to five seconds for the requested revision to become durable. Timeout cancels incomplete transfers, while a verified candidate remains available for a later CLI activation. Abort discards a candidate but cannot undo activation. Product mode policy permits critical periodic TX while suppressing ordinary telemetry/TX, using generation to invalidate old work. The bilingual update overlay replaces live readings; percentage represents received bytes only. Diagnostics, ISO-TP/UDS, Update and the NVM barrier keep running. App alone controls maintenance entry/exit. External cyclic senders may stop for a dedicated speed run; coexistence and normal HIL retain their original thresholds.

~~~text
METER_CAN_UPDATE=1
METER_UPDATE_VERSION=ota-board-a
meter info
meter storage
meter_update info
meter_update maintenance on
python -m tools.ota --evidence evidence/ota-probe probe
python -m tools.ota pack candidate.itb ota-output --product reference-demo --hardware reference-board --version ota-board-b --os-file d13x_os.itb --candidate-capacity 4194304 --pad-os-to 4096
python -m tools.ota --evidence evidence/ota-download download ota-output/ota.cpio --manifest ota-output/ota.manifest.json --os-file d13x_os.itb --candidate-capacity 4194304
python -m tools.ota --evidence evidence/ota-activate activate --version ota-board-b --reboot
~~~

After flashing, first verify UI, touch, meter info, NVM READY and backend_supported=true without enabling maintenance. Then enable maintenance for controlled testing. Preserve UART startup including native slot selection, PCAN ASC, package hash, before/after firmware version and update diagnostics. Wrong Product, damaged package, interruption, explicit Abort, maintenance entry/exit, UART responsiveness and post-update normal CAN recovery require separate evidence. A human-controlled power cut is a separate test; neither this build nor Host PASS establishes its result.

## Hardware transport observations

The first board session exposed two integration issues: the synchronous UDS client waits for a complete ISO-TP response, while the negotiated 150 ms P2 budget is shorter than a segmented diagnostic JSON response at 5 ms STmin; the RT-Thread console also truncates a single long formatted write. The Host connection now uses an explicit bounded two-second complete-response budget and preserves the 120-second response-pending limit. This is a client assembly budget, not a claim about ECU P2 compliance. Serial diagnostics emit bounded chunks. A regression performs session negotiation before receiving a long real ISO-TP response, and the adapter test uses a 128-byte console buffer.

CAN evidence preserves native backend RX timestamps; Host TX observations may have a different clock origin. Use ordered log windows and frame identities for correlation, and calculate RX intervals within one native clock. Do not subtract native uptime timestamps from Host epoch time. Periodic background traffic shares one python-can owner and stops before channel shutdown.

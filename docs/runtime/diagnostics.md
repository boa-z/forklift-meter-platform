# Diagnostics and serial debugging

## Responsibility and ownership

Log is for important human-readable events, Diagnostics for current state/counters, and Trace for recent structured history. UART remains RT-Thread Console; FinSH/MSH owns command tokenization; ULog owns filtering, formatting and asynchronous output. No new logger, shell, UART protocol or print queue is introduced.

The portable diagnostics library depends only on public contracts. The application statically owns its diagnostics object and binds Core and Runtime; protocol services pass the optional pointer to PDO/SDO adapters. All mutations and snapshot copies must be serialized by the caller. Unbound instrumentation is a no-op. Host programs can bind and query the same API without RT-Thread, LVGL or the SDK.

The production port publishes owner snapshots under short RT-Thread priority-mutex locks. MSH formats and writes UART after copying, outside publication locks; a separate command mutex protects query buffers. Queries do not sample devices or transmit CAN. Touch counters are copied by the LVGL owner after its timer handler.

Protocol, App/Core, LVGL UI, CAN TX, NVM and Update are separate owners connected through native IPC. The portable diagnostics object still requires caller serialization; board queries use published copies rather than concurrent access to live Core/Protocol state. See runtime-production.md for lifecycle and owner details.

## Counters and snapshot semantics

Runtime and board-driver counters describe different boundaries. Runtime tracks accepted/dispatched frames and resets; board CAN diagnostics report physical-bus RX, driver TX success/failure and native drops. Queue admission is separate from driver completion and does not prove remote acceptance. TX busy is admission refusal, not itself a controller fault. Compare deltas over explicitly bounded observation windows.

Domain counts accepted updates, including unchanged values; revision changes only for actual Domain changes. Source and validity transitions are counted separately. Reading diagnostics or counters never increments revision. Parameters/settings/fault metadata is read from the public Domain snapshot, not private core fields. Signal lookup uses canonical keys and copies the value; key/unit strings borrow the immutable Product catalog lifetime. Age is unsigned millisecond subtraction, valid within one uint32 clock cycle. UNKNOWN is reported explicitly; its numeric age is not evidence of received data.

PDO reports configured bindings, valid-data freshness and aggregate traffic/errors. SDO counters count queued logical requests and started/completed/aborted/timed-out **attempts**; retries are additional attempts. A successful retry does not erase earlier timeouts. Queue depth counts requests waiting to start (excluding the active transfer and retained terminal results); active_request=0 means no current transfer. Last nonzero abort code survives later success. The current diagnostic snapshot describes one SDO channel and one PDO aggregate; Products with multiple SDO channels must extend this schema before claiming per-channel visibility.

UI present_count is observed at presentation; flush_count comes from lvgl-aic. Touch exposes cached counters and coordinates. Storage reports the selected backend and RAM/in-flight/durable revisions; only absent backends report unavailable. slow_frame is not implemented. New counters saturate at UINT32_MAX; legacy Runtime counters retain their existing arithmetic.

## Trace and logging policy

The static ring holds 256 entries of 16 bytes (4096 bytes plus 16-byte metadata). Append is O(1), uses no heap and formats no strings. Snapshot returns insertion order, including uint32 timestamp wrap; short buffers select the newest entries. Overwrite count saturates. Clear removes visible history and overwrite count, retaining the consumer sequence. Queries never transmit CAN or change vehicle settings; trace clear is the only mutating command and prints the actual target/count/result.

Runtime/CAN/PDO/SDO/Domain event identifiers are defined in the portable header. Product READY/FAILED marks generic service outcome; it contains no customer workflow. SDO QUEUE arg0 is request_id, arg1 packs node[31:24], index[23:8], subindex[7:0]. START/RETRY arg1 is the attempt; terminal arg1 is the abort code. Domain arg0 is signal identity. CAN arg0 is bus; other details remain numeric and inspectable in dump output.

Normal CAN RX/TX, PDO RX, Touch samples and UI frames increment counters only. State transitions/errors enter Trace. The RT-Thread backend maps selected records to native ULog tags: meter.runtime, meter.can, meter.pdo, meter.sdo, meter.product; initialization uses meter.boot/meter.board, and LVGL uses meter.ui. PDO stale/SDO timeout/invalid frames are WARN; PDO recovery/product ready are INFO; exhausted SDO requests and terminal startup service failure are ERROR. SDO_FAILED is emitted only after retry policy is exhausted. Repeated warnings are limited to one per module per second; counters and trace still record them. Debug messages obey native ULog settings. The existing SDK uses INFO and asynchronous Console output.

The backend drains trace once per owner iteration. Ring overwrite or explicit clear can discard not-yet-consumed log events; trace/log are bounded debugging evidence, not a reliable event-delivery bus. Clear before drain may suppress that historical log. UART queries may interleave with asynchronous logs; raw capture preserves what was actually received. MSH trace copy and log-consumer copy each use another static 4096-byte buffer; total trace-related BSS is about 12 KiB, not just the history ring.

## MSH queries

```text
meter info
meter diag
meter runtime
meter_exec
meter storage
meter can
meter can 1
meter pdo
meter sdo
meter domain
meter signal vehicle.speed
meter touch
meter trace
meter trace dump
meter trace clear
```

Invalid or extra arguments print usage without mutation. SDO read/write commands are intentionally not implemented. CAN live capture is reserved for a future bounded, filtered static buffer (bus/id/count); there is no unbounded dump hook and no RX callback UART output. signal not found is explicit; a known Demo key is vehicle.speed. info prints Product, platform SHA, SDK SHA, LVGL version, lvgl-aic revision, board, build date/time and uptime.

Build identity is generated by tools/build_identity.py through SCons. The generated header also names the selected Product through METER_BUILD_PRODUCT (manifest identity.id, otherwise meter_product_t.id) and its revision through METER_BUILD_PRODUCT_REVISION, so an archived header states which Product composed the image. Clean repositories report full SHA; dirty repositories append a content/status fingerprint including untracked nonignored source. The board name is normalized from Kconfig. Source SHA plus image SHA256 binds evidence; build date alone is insufficient. An inaccessible repository is marked unavailable. The SDK may be dirty because a separate private submodule is dirty; do not clean another task's files just to remove this marker.

## Host capture and evidence

```sh
python -m pip install -r tools/serial/requirements.txt
python tools/serial/meter_serial.py --port /dev/ttyUSB0 capture evidence/serial.log
python tools/serial/meter_serial.py --port /dev/ttyUSB0 command "meter info" --output evidence/info.txt
python tools/serial/meter_serial.py --port /dev/ttyUSB0 command "meter diag" --output evidence/diag.txt
python tools/serial/meter_serial.py --port /dev/ttyUSB0 command "meter runtime" --output evidence/runtime.txt
python tools/serial/meter_serial.py --port /dev/ttyUSB0 command "meter can" --output evidence/can.txt
python tools/serial/meter_serial.py --port /dev/ttyUSB0 command "meter sdo" --output evidence/sdo.txt
python tools/serial/meter_serial.py --port /dev/ttyUSB0 command "meter trace dump" --timeout 10 --idle 1 --output evidence/trace.txt
```

On Windows replace /dev/ttyUSB0 with the explicit COM port. Use the same Python interpreter for installing requirements and running the helper. Files append raw bytes, including ANSI escapes and malformed UTF-8; the helper does not interpret protocol success. capture runs until Ctrl-C or --duration seconds. command stops after --idle silence following data or the --timeout deadline; silence exits with failure. Long dumps need an adequate deadline. An echoed command alone is not proof that the board executed it. Control lines are configured inactive before opening, but adapter/OS behavior must be considered when reset is tied to DTR/RTS.

Only one process may own the board serial port: stop capture before running separate command invocations, or use a single external terminal with session logging. Do not run the listed capture and command processes concurrently. To capture boot, start capture before the operator resets the board. Save the whole boot log and each command response. The helper has no implicit flashing or reboot action.

Before flashing, reserve the shared reference-board board using SDK project-management policy and record its restore image. Copy the exact generated image to an evidence directory and save SHA256 (PowerShell Get-FileHash or Linux sha256sum). Save the generated build identity header, .config, image/map/ELF and SDK/platform revisions next to it. Then obtain boot log, info.txt, diag.txt, runtime.txt, can.txt, sdo.txt and trace.txt from the physical board. Also query domain, a known signal and touch; exercise the CAN fixture and verify PDO stale/recovery and SDO timeout/retry/abort on the appropriate Mixed image. Never manufacture board logs from Host fixtures.

## Validation and limits

HOST_PASS requires Demo, Reference-B, Reference-Mixed, public headers, architecture, diagnostics, trace, command rendering, production MSH frontend tests, serial helper and build identity tests. The MSH host test replaces only native RT API/registration and asserts output occurs outside the owner mutex; it also verifies native ULog severity, warning rate limits and silence for normal PDO traffic. The serial tests include pyserial loopback; neither establishes physical UART behavior. Linux is the CI default.

IMAGE_READY requires the selected SDK firmware build plus artifact hash. BOARD_PASS additionally requires physical boot/MSH/fixture evidence tied to that image. Current work reports BOARD_NOT_RUN when no board port is available. Demo firmware has no CANopen adapter, so pdo/sdo unavailable is correct; Mixed host tests prove its SDO/PDO diagnostics, not the Demo image's hardware behavior. This task does not add Mixed firmware selection, persistent trace, coredump, network logging, custom UART parser or extra Demo pages.

See the [RT-Thread adapter](../../platform/rtthread/README.md) and [CANopenNode integration](protocols.md) for adjacent architecture.

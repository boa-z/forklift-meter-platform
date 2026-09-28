# CAN HIL automation

## Scope and status

The first gate targets the synthetic reference-demo on D50T-2-Lite: CAN PHY, driver, Runtime, decoder and Domain. UI/touch remain separate manual gates. Reference-Mixed HIL remains NOT_RUN until firmware Product selection, RT-Thread CAN TX and a Mixed image are verified.

HOST_PASS means parser/vector/virtual-bus tests passed. HIL_NOT_RUN means hardware was not exercised or the selected run was skipped/incomplete. HIL_PASS means the selected physical tests passed; inspect recorded test names for full gate coverage. HIL_FAIL preserves failures. Zero pytest exit status with skipped hardware tests is not HIL_PASS.

## Dependencies and design

tools/hil/requirements.txt reuses cantools 40.7.1 (MIT), python-can 4.6.1 (LGPL-3.0), PyYAML 6.0.3 (MIT) and pySerial 3.5 (BSD-3-Clause). New pytest 8.3.5 (MIT) provides assertions/JUnit; filelock 3.18.0 (Unlicense) provides portable process ownership. Sources are PyPI distributions and upstream projects. All dependencies are PC-only: no firmware RAM/Flash cost. Windows additionally needs PEAK PCAN-Basic installed. This project adds no direct PEAK C binding.

Normal frames use cantools encode_message(strict=True) exclusively. Tests describe physical values, never repeat byte layouts. Wrong-DLC and unknown-ID tests may construct can.Message directly. Metadata generates deterministic representable min, min+LSB, nominal, max-LSB, max, signed crossing-zero values and all defined enum values. IEEE float signals need a future explicit vector policy. Vectors are saved before execution.

DutSession reuses pySerial and the existing serial command writer. One reader continuously saves exact received bytes, including asynchronous logs and invalid UTF-8. Commands require complete stable fields within a deadline; echo, ANSI and prompts do not count as success. Per-port filelock and native exclusivity limit ownership. Release external terminals first. No flashing or reboot is performed.

CanBus delegates transport, notification and periodic scheduling to python-can. Multiple instances support CAN0/CAN1 with separate logs; Host tests verify isolation and TX/RX. ASC uses host observation time. RX records receipt; direct TX records backend acceptance; periodic callbacks record TX attempts before backend completion. DUT counters establish reception: TX logs alone prove neither reception nor ACK. Sender-thread/listener errors fail tests.

## Commands

Run from the Platform root. Ordinary pytest runs Host utilities and skips physical tests. Physical tests require --hil and an explicit UART port. Missing UART/CAN backend produces SKIP and HIL_NOT_RUN; an unresponsive or wrong DUT fails. Virtual CAN cannot produce HIL_PASS. SocketCAN uses the same API and requires operator configuration of the network interface.

~~~sh
python -m pip install -r tools/hil/requirements.txt
python -m pytest -q
python -m tools.hil.dbc products/demo/protocol/can/demo.dbc motion speed --nominal 25 --output evidence/speed-vectors.json
python -m pytest -m hil
python -m pytest -m hil --hil --can-interface pcan --can-channel PCAN_USBBUS1 --can-bitrate 500000 --dut-port COM11
python -m pytest -m hil --hil --can-interface socketcan --can-channel can0 --can-bitrate 500000 --dut-port /dev/ttyUSB0
~~~

Connect only the intended test board. Stop PCAN-View transmit lists and release UART terminals. PCAN-View remains manual inspection/capture only. Reserve the shared board under SDK policy. Before CAN stimulus, the harness checks reference-demo, d50t-2-lite and CAN0 bitrate. Tests clear recent Trace, inject synthetic signals and stop all tasks afterward; they do not change settings. The final board may become STALE when traffic stops.

--hil-evidence selects the evidence root; --hil-image records the flashed image SHA256 (otherwise NOT_PROVIDED). Firmware-reported identity is always captured when reachable. --dut-baud defaults to 115200. Do not run physical HIL with pytest-xdist: UART ownership is exclusive.

## Gates

| Gate | Oracle |
|---|---|
| HIL-01 | CAN RX and accepted/dispatched increase; correct VALID speed/SOC |
| HIL-02 | Deterministic speed/SOC vectors; Domain value/state |
| HIL-03 | Stop beyond stale_ms, then recover; transition counters and signal Trace |
| HIL-04 | Unknown ID increments unrouted once; normal decoding continues |
| HIL-05 | Known-ID DLC7 increments decode_failed once, not malformed; INVALID_FRAME Trace |
| HIL-06 | Five frames every 10ms for 2s; at least 500 RX; no new overflow/drop/error/reset |

The D50T burst gate also saves native RT-Thread canstat before/after and asserts zero native receive drops. This exposes losses before the meter Runtime boundary. burst-summary.json separates Host scheduled TX, native driver RX/drop and Domain-path RX/dispatched. The native canstat extension is specific to RT-Thread; other DUT frontends must supply their own equivalent before reusing this gate.

Counters use deltas rather than lifetime totals. A physically valid short frame is a decoder error; this gate does not inject impossible DLC>8, bad CRC or bus-off. The first boundary gate covers required speed/SOC. The known generated-float height=6m error is not hidden or claimed fixed by this gate. Settings/SDO/PDO and camera assertions are out of scope.

## Evidence

Every explicit HIL run creates evidence/hil/UTC-ID with metadata.json, meter-info.txt, serial.log (raw bytes), can.asc, diag-before.txt, diag-after.txt, trace.txt and pytest-generated junit.xml. Per-test diagnostics/Trace, boundary-vectors.json and failure dumps add detail. A custom --junitxml location is recorded. Evidence is Git-ignored and must be retained separately.

Metadata records firmware Platform SHA, SDK identity, lvgl-aic revision, board, Host SHA, CAN interface/channel/bitrate, UART port, DBC SHA256, scenario, image hash availability and test-phase outcomes. Failure capture attempts diag/runtime/can/domain/trace independently and records collection errors. Interrupted runs cannot become HIL_PASS. Missing hardware may prevent some files from being populated; reasons remain explicit.

python-can ASCReader reads generated evidence and LogReader supports later PCAN TRC fixtures. Future replay must distinguish observed RX and injected TX; normal synthetic scenarios remain DBC-driven. Customer DBCs/traces must never enter this public repository. This phase adds neither a replay engine nor a CANopen server.

## Validation and limits

Linux CI runs pytest and existing Demo/Reference-B/Reference-Mixed CMake/CTest. Host tests cover malformed scenario rejection, full Demo vector representability, enums, UART completeness/deadline, raw bytes, ownership, bidirectional virtual CAN, two-channel isolation and failure evidence. These are separate from board results.

The initial YAML format contains only name, period_ms and message/signals entries. Timing, loops and complex assertions use pytest Python rather than a DSL. CAN1 can use another CanBus instance, without claiming Mixed firmware availability. ASC TX attempt timestamps are scheduling observations, not hardware timestamps. Burst is bounded traffic, not a saturation guarantee. The height boundary error requires a separate generator fix and firmware rebuild.

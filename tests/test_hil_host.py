"""Host 单元测试：解析器、边界向量、虚拟 CAN 和证据，不宣称板测通过。"""
import json
from pathlib import Path
import queue
import subprocess
import sys
import time
import uuid
import can
import pytest
from tools.hil.dbc import Dbc
from tools.hil.dut import DutSession, complete, diagnostics, info, signal, trace, driver_status
from tools.hil.evidence import Evidence
from tools.hil.scenario import load, transmitting
from tools.hil.can_bus import CanBus

ROOT = Path(__file__).resolve().parents[1]
DBC = ROOT/'products/demo/protocol/can/demo.dbc'
SCENARIO = ROOT/'tests/hil/scenarios/normal.yaml'


def test_scenario_encoding_and_validation(tmp_path):
    dbc = Dbc(DBC)
    data = load(SCENARIO, dbc)
    msg = dbc.encode(data['messages'][0]['message'], data['messages'][0]['signals'])
    decoded = dbc.database.decode_message(msg.arbitration_id, msg.data)
    assert decoded['speed'] == 25 and decoded['steering'] == -45
    for text in ['name: a\nperiod_ms: .nan\nmessages: []', 'name: a\nloop: true', 'name: a\nperiod_ms: -1\nmessages: []']:
        path = tmp_path/'bad.yaml'
        path.write_text(text)
        with pytest.raises(ValueError): load(path, dbc)
    with pytest.raises(cantools_error()): dbc.encode('motion', {'speed': 99})


def cantools_error():
    import cantools
    return cantools.database.errors.EncodeError


def test_vectors_deterministic_and_representable():
    dbc = Dbc(DBC)
    data = load(SCENARIO, dbc)
    for row in data['messages']:
        for name, nominal in row['signals'].items():
            vectors = dbc.vectors(row['message'], name, nominal)
            assert vectors == dbc.vectors(row['message'], name, nominal)
            assert {v['label'] for v in vectors} >= {'min', 'max', 'nominal'}
            for vector in vectors:
                msg = dbc.encode(row['message'], row['signals'] | {name: vector['value']})
                value = dbc.database.decode_message(msg.arbitration_id, msg.data)[name]
                assert value == pytest.approx(vector['value'])
    steering = dbc.vectors('motion', 'steering', 0)
    assert {v['value'] for v in steering} >= {-.01, 0, .01, -45, 45}
    assert [v['value'] for v in dbc.vectors('energy', 'soc', 78)] == [0, 1, 78, 99, 100]


def test_enum_vectors(tmp_path):
    path = tmp_path/'enum.dbc'
    path.write_text('VERSION ""\nNS_ :\nBS_:\nBU_: Sender\nBO_ 1 states: 1 Sender\n SG_ mode : 0|8@1+ (1,0) [0|255] "" Sender\nVAL_ 1 mode 0 "off" 2 "ready" 7 "fault";\n')
    assert [v['value'] for v in Dbc(path).vectors('states', 'mode')] == [0, 2, 7]


def test_uart_parsers():
    text = '\x1b[32maic /> meter domain\r\n[123] I/log: junk=bad\r\ndomain updates=42 stale=3 valid=7 source_switch=0 error=0\r\naic /> '
    assert diagnostics(text) == {'domain': dict(updates=42, stale=3, valid=7, source_switch=0, error=0)}
    assert info('product : reference-demo\r\nplatform : abc\r\n') == {'product': 'reference-demo', 'platform': 'abc'}
    response = 'signal id=1 key=vehicle.speed value=12.3 unit=km/h\r\nstate=VALID source=1 timestamp_ms=1 age_ms=2 stale_ms=750\r\n'
    assert signal(response, 'vehicle.speed')['value'] == 12.3
    assert complete('meter signal vehicle.speed', response)
    assert not complete('meter signal vehicle.speed', response.splitlines()[0])
    with pytest.raises(ValueError): signal(response, 'energy.soc')
    with pytest.raises(ValueError): signal(response.replace('12.3', 'nan'), 'vehicle.speed')
    dump = 'trace entries=1 overwritten=0\r\n123 DOMAIN STALE module=5 event=1 arg0=1 arg1=0x00000000\r\n'
    assert trace(dump)[0]['event'] == 1
    assert not complete('meter trace dump', dump.splitlines()[0])
    assert not complete('meter info', 'aic /> meter info\r\n')


def test_virtual_can_bidirectional_and_asc(tmp_path):
    channel = 'hil-'+uuid.uuid4().hex
    bus = CanBus('virtual', channel, 500000, tmp_path/'can.asc', bus_index=1)
    peer = can.Bus(interface='virtual', channel=channel, ignore_config=True)
    dbc = Dbc(DBC)
    scenario = load(SCENARIO, dbc)
    try:
        with transmitting(bus, dbc, scenario):
            assert peer.recv(1) is not None
        while peer.recv(.03) is not None: pass
        assert peer.recv(.15) is None
        message = dbc.encode('lift', {'height': 3})
        peer.send(message)
        time.sleep(.1)
        bus.send(message)
        assert peer.recv(1).data == message.data
    finally:
        bus.close()
        peer.shutdown()
    with can.ASCReader(str(tmp_path/'can.asc')) as reader:
        messages = list(reader)
    assert any(m.is_rx for m in messages) and any(not m.is_rx for m in messages)
    assert all(m.channel == 1 for m in messages)


def test_two_bus_isolation(tmp_path):
    first = 'hil-'+uuid.uuid4().hex
    second = 'hil-'+uuid.uuid4().hex
    a = CanBus('virtual', first, 500000, tmp_path/'can0.asc', 0)
    b = CanBus('virtual', second, 500000, tmp_path/'can1.asc', 1)
    peer = can.Bus(interface='virtual', channel=second, ignore_config=True)
    try:
        a.send(can.Message(arbitration_id=1, data=[1], is_extended_id=False))
        assert peer.recv(.1) is None
        b.send(can.Message(arbitration_id=2, data=[2], is_extended_id=False))
        assert peer.recv(.5).arbitration_id == 2
    finally:
        a.close(); b.close(); peer.shutdown()


def test_failure_capture_continues(tmp_path):
    evidence = Evidence(tmp_path)
    class Broken:
        def command(self, text, **kwargs):
            if text == 'meter runtime': raise TimeoutError('missing')
            return text+'\r\n'
    errors = evidence.capture(Broken())
    assert errors == {'meter runtime': 'missing'}
    assert (evidence.path/'failure-trace.txt').exists()
    assert json.loads((evidence.path/'metadata.json').read_text())['status'] == 'HIL_NOT_RUN'


def test_dut_capture_timeout_and_ownership(tmp_path, monkeypatch):
    class Port:
        def __init__(self): self.rx = queue.Queue(); self.is_open = False
        @property
        def in_waiting(self): return 1
        def open(self): self.is_open = True
        def close(self): self.is_open = False
        def flush(self): pass
        def read(self, n):
            try: return self.rx.get(timeout=.01)
            except queue.Empty: return b''
        def write(self, data):
            if data.startswith(b'meter signal'):
                self.rx.put(b'signal id=1 key=vehicle.speed value=25 unit=km/h\r\n')
                self.rx.put(b'state=VALID source=1 timestamp_ms=10 age_ms=0 stale_ms=750\r\n')
            else: self.rx.put(data)
            return len(data)
    port = Port()
    monkeypatch.setattr('tools.hil.dut.serial.serial_for_url', lambda *a, **k: port)
    dut = DutSession('test-'+uuid.uuid4().hex, tmp_path/'serial.log', timeout=.15)
    try:
        from filelock import FileLock, Timeout
        with pytest.raises(Timeout): FileLock(dut.lock.lock_file).acquire(timeout=0)
        port.rx.put(b'\xffBOOT\r\n')
        time.sleep(.03)
        assert dut.signal('vehicle.speed')['state'] == 'VALID'
        with pytest.raises(TimeoutError): dut.command('meter info')
        with pytest.raises(ValueError): dut.command('meter info\nreboot')
    finally: dut.close()
    assert (tmp_path/'serial.log').read_bytes().startswith(b'\xffBOOT\r\n')
    assert not port.is_open


def test_virtual_hil_is_skip_not_physical_pass(tmp_path):
    result = subprocess.run([sys.executable, '-m', 'pytest', 'tests/hil', '--hil', '--can-interface', 'virtual',
                             '--hil-evidence', str(tmp_path), '-q'], cwd=ROOT, text=True, capture_output=True, timeout=30)
    assert result.returncode == 0, result.stdout+result.stderr
    metadata = json.loads(next(tmp_path.glob('*/metadata.json')).read_text())
    assert metadata['status'] == 'HIL_NOT_RUN'
    assert len([t for t in metadata['tests'] if t['outcome'] == 'skipped']) == 6
    assert not any(metadata['coverage'].values())
    assert next(tmp_path.glob('*/junit.xml')).exists()


def test_vector_cli_reproduction(tmp_path):
    output = tmp_path/'vectors.json'
    subprocess.run([sys.executable, '-m', 'tools.hil.dbc', str(DBC), 'motion', 'speed', '--nominal', '25', '--output', str(output)], cwd=ROOT, check=True, timeout=10)
    data = json.loads(output.read_text())
    assert data['dbc_sha256'] == Dbc(DBC).sha256
    assert [v['value'] for v in data['vectors']] == [0, .01, 25, 49.99, 50]


def test_vector_negative_scale_and_non_aligned_limits(tmp_path):
    path = tmp_path/'negative.dbc'
    path.write_text('VERSION ""\nNS_ :\nBS_:\nBU_: Sender\nBO_ 1 data: 1 Sender\n SG_ value : 0|8@1- (-0.5,1) [-2.1|2.1] "" Sender\n')
    vectors = Dbc(path).vectors('data', 'value', 0)
    assert [v['value'] for v in vectors[:5]] == [-2, -1.5, 0, 1.5, 2]
    assert {v['value'] for v in vectors} >= {-.5, 0, .5}


def test_native_canstat_parser():
    text = 'Total.receive.packages: 0000001200. Dropped.receive.packages: 0000000100.\nTotal..send...packages: 0000000000. Dropped...send..packages: 0000000000.\n'
    assert driver_status(text) == dict(rx=1200, drop=100, tx=0, tx_drop=0)
    assert complete('canstat can0', text)
    assert not complete('canstat can0', text.splitlines()[0])


def test_uart_fragmented_last_field_requires_newline():
    prefix = 'product : reference-demo\nplatform : abc\nsdk : def\nlvgl-aic : 123\nboard : reference-board\n'
    for ending in ['uptime_ms : ', 'uptime_ms : 1', 'uptime_ms : 1234 ', 'uptime_ms : \n']:
        assert not complete('meter info', prefix+ending)
    assert complete('meter info', prefix+'uptime_ms : 1234\n')
    assert info('uptime_ms : \n') == {}

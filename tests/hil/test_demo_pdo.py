"""真实仪表发送 PDO：公开合成载荷、新鲜度、计数及恢复；不模拟目标机发送。"""
import hashlib
import json
from pathlib import Path
import threading
import time
import cantools
import pytest
from tools.hil.scenario import transmitting

pytestmark = pytest.mark.hil


def test_demo_instrument_pdo(hil):
    path = Path(__file__).resolve().parents[2] / 'products/demo/protocol/can/demo_tx.dbc'
    database = cantools.database.load_file(path)
    samples = []
    lock = threading.Lock()
    overrides = dict(motion=dict(speed=12.5), energy=dict(soc=75, charging=0),
                     lift=dict(height=1.25), load=dict(weight=780),
                     status=dict(seat=1, brake=0, neutral=1, warning=1))

    def receive(message):
        if message.arbitration_id in (0x381, 0x481):
            with lock:
                samples.append(dict(id=message.arbitration_id, timestamp=message.timestamp,
                                    extended=message.is_extended_id, remote=message.is_remote_frame,
                                    data=list(message.data)))

    def captured():
        with lock:
            return list(samples)

    def tail(ident):
        rows = [row for row in captured() if row['id'] == ident][-3:]
        assert len(rows) == 3, rows
        return [database.decode_message(ident, bytes(row['data'])) for row in rows]

    hil.bus.notifier.add_listener(receive)
    try:
        with transmitting(hil.bus, hil.dbc, hil.scenario, overrides, period_ms=10):
            time.sleep(2)
            for row in tail(0x381):
                assert row['fresh'] == 1
                assert (row['speed'], row['height'], row['load'], row['soc']) == (12.5, 1.25, 780, 75)
            for row in tail(0x481):
                assert row['fresh'] == 1 and row['layout_version'] == 1 and row['generation'] > 0
                assert (row['seat'], row['brake'], row['neutral'], row['charging'], row['warning']) == (1, 0, 1, 0, 1)
        # 输入停止后，重复 App 发布不可让 PDO 继续声称新鲜。
        time.sleep(1.1)
        for row in tail(0x381):
            assert row['fresh'] == 0
            assert (row['speed'], row['height'], row['load'], row['soc']) == (0, 0, 0, 0)
        for row in tail(0x481):
            assert row['fresh'] == 0
            assert (row['seat'], row['brake'], row['neutral'], row['charging'], row['warning']) == (0, 0, 0, 0, 0)
        zero = dict(motion=dict(speed=0), energy=dict(soc=0, charging=0), lift=dict(height=0),
                    load=dict(weight=0), status=dict(seat=0, brake=0, neutral=0, warning=0))
        with transmitting(hil.bus, hil.dbc, hil.scenario, zero, period_ms=10):
            time.sleep(.8)
            for ident in (0x381, 0x481):
                assert all(row['fresh'] == 1 for row in tail(ident))
            assert all(row['speed'] == row['height'] == row['load'] == row['soc'] == 0 for row in tail(0x381))
        for ident, modulus in ((0x381, 128), (0x481, 256)):
            rows = [row for row in captured() if row['id'] == ident]
            assert len(rows) >= 30, rows
            assert all(not row['extended'] and not row['remote'] and len(row['data']) == 8 for row in rows)
            assert all(b['timestamp'] > a['timestamp'] for a, b in zip(rows, rows[1:]))
            values = [database.decode_message(ident, bytes(row['data'])) for row in rows]
            assert all(b['sequence'] == (a['sequence'] + 1) % modulus for a, b in zip(values, values[1:]))
    finally:
        hil.bus.notifier.remove_listener(receive)
        hil.evidence.text('demo-pdo.json', json.dumps(dict(tx_dbc_sha256=hashlib.sha256(path.read_bytes()).hexdigest(),
                                                        frames=captured()), indent=2))

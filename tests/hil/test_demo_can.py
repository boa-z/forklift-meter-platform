"""实板 CAN PHY 到 Domain 的首批自动判定，不使用 UI 图像。"""
import json
import time
import can
import pytest
from tools.hil.dut import diagnostics, trace, driver_status
from tools.hil.scenario import transmitting

pytestmark = pytest.mark.hil


def counters(hil):
    """一次快照获取 CAN/Runtime/Domain，避免计数来源混淆。"""
    return diagnostics(hil.dut.command('meter diag'))


def assert_clean(before, after):
    """正常激励下不允许新增队列溢出、丢帧和硬件错误。"""
    for section, keys in [('runtime', ['overflow', 'resets', 'malformed', 'decode_failed', 'unrouted']),
                          ('can0', ['drop', 'rx_error', 'tx_error'])]:
        for key in keys:
            assert after[section][key] == before[section][key], (section, key, before, after)


def assert_signal(hil, key, value):
    """给处理线程有限调度余量，以内部状态验证正确数值。"""
    deadline = time.monotonic()+2
    while True:
        actual = hil.dut.signal(key)
        if actual['state'] == 'VALID' and actual['value'] == pytest.approx(value, abs=.00001):
            return actual
        if time.monotonic() >= deadline:
            assert actual['state'] == 'VALID', actual
            assert actual['value'] == pytest.approx(value, abs=.00001), actual
        time.sleep(.03)


def test_hil01_normal(hil):
    before = counters(hil)
    with transmitting(hil.bus, hil.dbc, hil.scenario):
        time.sleep(.3)
        assert_signal(hil, 'vehicle.speed', 25)
        assert_signal(hil, 'energy.soc', 78)
        after = counters(hil)
        assert after['can0']['rx'] > before['can0']['rx']
        for key in ('accepted', 'dispatched'):
            assert after['runtime'][key] > before['runtime'][key]
        assert_clean(before, after)


def test_hil02_boundaries(hil):
    vectors = []
    for message, signal, nominal, key in [('motion', 'speed', 25, 'vehicle.speed'), ('energy', 'soc', 78, 'energy.soc')]:
        vectors.extend(v | {'domain': key} for v in hil.dbc.vectors(message, signal, nominal))
    hil.evidence.text('boundary-vectors.json', json.dumps(vectors, indent=2))
    for vector in vectors:
        with transmitting(hil.bus, hil.dbc, hil.scenario, {vector['message']: {vector['signal']: vector['value']}}):
            assert_signal(hil, vector['domain'], vector['value'])


def test_hil04_unknown_id(hil):
    known = {m.frame_id for m in hil.dbc.database.messages}
    unknown = next(i for i in range(0x700, 0x7ff) if i not in known)
    with transmitting(hil.bus, hil.dbc, hil.scenario):
        assert_signal(hil, 'vehicle.speed', 25)
        before = counters(hil)
        hil.bus.send(can.Message(arbitration_id=unknown, is_extended_id=False, data=[0]*8, check=True))
        time.sleep(.2)
        after = counters(hil)
        assert after['runtime']['unrouted'] == before['runtime']['unrouted']+1
        assert after['runtime']['decode_failed'] == before['runtime']['decode_failed']
        assert_signal(hil, 'vehicle.speed', 25)


def test_hil05_wrong_dlc(hil):
    definition = hil.dbc.database.get_message_by_name('motion')
    with transmitting(hil.bus, hil.dbc, hil.scenario):
        assert_signal(hil, 'vehicle.speed', 25)
        before = counters(hil)
        hil.bus.send(can.Message(arbitration_id=definition.frame_id, is_extended_id=False, data=[0]*(definition.length-1), check=True))
        time.sleep(.2)
        after = counters(hil)
        assert after['runtime']['decode_failed'] == before['runtime']['decode_failed']+1
        assert after['runtime']['malformed'] == before['runtime']['malformed']
        records = trace(hil.dut.trace_dump())
        assert any(r['module'] == 1 and r['event'] == 5 and r['arg0'] == definition.frame_id for r in records)
        assert_signal(hil, 'vehicle.speed', 25)


def test_hil06_burst(hil):
    native_before_text = hil.dut.command('canstat can0')
    hil.evidence.text('burst-driver-before.txt', native_before_text)
    before = counters(hil)
    host_before = hil.bus.counters()
    start = time.monotonic()
    # 五帧各 10ms，共约 500 帧/秒；该有界压力档明确要求零溢出/丢帧。
    with transmitting(hil.bus, hil.dbc, hil.scenario, period_ms=10):
        time.sleep(2)
        assert_signal(hil, 'vehicle.speed', 25)
        after = counters(hil)
        host_after = hil.bus.counters()
    native_after_text = hil.dut.command('canstat can0')
    hil.evidence.text('burst-driver-after.txt', native_after_text)
    native_before, native_after = driver_status(native_before_text), driver_status(native_after_text)
    summary = dict(duration_s=time.monotonic()-start, period_ms=10,
                   host_tx_scheduled=host_after['tx_scheduled']-host_before['tx_scheduled'],
                   dut_rx=after['can0']['rx']-before['can0']['rx'],
                   dut_dispatched=after['runtime']['dispatched']-before['runtime']['dispatched'],
                   driver_rx=native_after['rx']-native_before['rx'],
                   driver_drop=native_after['drop']-native_before['drop'],
                   before=before, after=after)
    hil.evidence.text('burst-summary.json', json.dumps(summary, indent=2))
    assert summary['host_tx_scheduled'] >= 900, 'Host did not generate requested burst'
    assert after['can0']['rx']-before['can0']['rx'] >= 500
    assert after['runtime']['dispatched'] > before['runtime']['dispatched']
    assert_clean(before, after)
    assert summary['driver_drop'] == 0, 'native CAN driver dropped frames'

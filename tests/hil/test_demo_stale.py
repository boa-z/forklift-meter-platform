"""自动验证停止周期发送后的 STALE、恢复及对应内部 Trace。"""
import time
import pytest
from tools.hil.dut import diagnostics, trace
from tools.hil.scenario import transmitting

pytestmark = pytest.mark.hil


def test_hil03_stale_recovery(hil):
    with transmitting(hil.bus, hil.dbc, hil.scenario):
        time.sleep(.3)
        first = hil.dut.signal('vehicle.speed')
        assert first['state'] == 'VALID'
        before = diagnostics(hil.dut.command('meter domain'))['domain']
    time.sleep(first['stale_ms']/1000+.3)
    stale = hil.dut.signal('vehicle.speed')
    assert stale['state'] == 'STALE'
    middle = diagnostics(hil.dut.command('meter domain'))['domain']
    assert middle['stale'] > before['stale']
    records = trace(hil.dut.trace_dump())
    assert any(r['module'] == 5 and r['event'] == 1 and r['arg0'] == first['id'] for r in records)
    with transmitting(hil.bus, hil.dbc, hil.scenario):
        time.sleep(.3)
        recovered = hil.dut.signal('vehicle.speed')
        assert recovered['state'] == 'VALID' and recovered['value'] == pytest.approx(25)
        after = diagnostics(hil.dut.command('meter domain'))['domain']
        assert after['valid'] > middle['valid']
        records = trace(hil.dut.trace_dump())
        assert any(r['module'] == 5 and r['event'] == 2 and r['arg0'] == first['id'] for r in records)

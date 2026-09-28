"""板测前停止残留任务，清除本轮 Trace，并检测 DUT 重启。"""
import pytest
from tools.hil.dut import info


@pytest.fixture(autouse=True)
def test_evidence(hil, request):
    """每个用例保存前后诊断，记录独立测试窗口。"""
    hil.bus.stop_tasks()
    name = request.node.name
    identity = info(hil.dut.command('meter info'))
    hil.evidence.text(name+'-before.txt', hil.dut.command('meter diag'))
    hil.dut.command('meter trace clear')
    yield
    hil.bus.stop_tasks()
    hil.evidence.text(name+'-after.txt', hil.dut.command('meter diag'))
    hil.evidence.text(name+'-trace.txt', hil.dut.trace_dump())
    final = info(hil.dut.command('meter info'))
    assert final['platform'] == identity['platform']
    assert int(final['uptime_ms']) >= int(identity['uptime_ms']), 'DUT rebooted during test'

"""生产 Runtime 实板测量；保持既有 burst 接纳门槛。"""
import json
import statistics
import time
import pytest
from tools.hil.dut import diagnostics
from tools.hil.scenario import transmitting

pytestmark = pytest.mark.hil


def test_runtime_periodic_timing(hil):
    samples = {0x3c0: [], 0x2f0: []}
    def receive(message):
        if message.arbitration_id in samples and not message.is_extended_id:
            samples[message.arbitration_id].append(message.timestamp)
    hil.bus.notifier.add_listener(receive)
    before = hil.dut.command('meter_exec')
    try:
        with transmitting(hil.bus, hil.dbc, hil.scenario, period_ms=10):
            time.sleep(6)
            after = hil.dut.command('meter_exec')
    finally:
        hil.bus.notifier.remove_listener(receive)
    report = {}
    for ident, period in ((0x3c0, 50), (0x2f0, 100)):
        stamps = samples[ident]
        spacing = [(b-a)*1000 for a, b in zip(stamps, stamps[1:])]
        report[hex(ident)] = dict(samples=len(stamps), expected_ms=period,
            min_ms=min(spacing) if spacing else None, max_ms=max(spacing) if spacing else None,
            mean_ms=statistics.mean(spacing) if spacing else None,
            jitter_max_ms=max((abs(x-period) for x in spacing), default=None),
            long_gaps=sum(x >= period*1.5 for x in spacing))
    hil.evidence.text('runtime-periodic.json', json.dumps(report, indent=2))
    hil.evidence.text('runtime-before.txt', before)
    hil.evidence.text('runtime-after.txt', after)
    # 只判定覆盖和单调性；jitter 是测量值，不捏造车辆协议容差。
    for ident in samples:
        assert len(samples[ident]) >= 40, report
        assert all(b > a for a,b in zip(samples[ident], samples[ident][1:])), report


def settings_result(dut):
    """每次消费显式结果；PENDING 必须重新查询，不等待不存在的推送。"""
    deadline = time.monotonic() + 5
    while True:
        result = dut.command('meter_settings result')
        if 'PENDING_OR_NONE' not in result:
            return result
        assert time.monotonic() < deadline, result
        time.sleep(.02)


def test_runtime_nvm_under_load(hil):
    before = diagnostics(hil.dut.command('meter diag'))
    original = before['storage']['brightness']
    changed = 60 if original != 60 else 65
    try:
        with transmitting(hil.bus, hil.dbc, hil.scenario, period_ms=10):
            accepted = hil.dut.command('meter_settings brightness '+str(changed))
            assert 'result=QUEUED' in accepted
            result = settings_result(hil.dut)
            assert 'result=APPLIED_OR_QUEUED' in result
            deadline = time.monotonic()+12
            while True:
                after = diagnostics(hil.dut.command('meter diag'))
                row = after['storage']
                if row['brightness'] == changed and row['state'] in ('READY', 'DURABLE') and not row['dirty'] and row['durable_revision'] == row['ram_revision']:
                    break
                assert time.monotonic() < deadline, after
                time.sleep(.2)
            hil.evidence.text('runtime-nvm-load.json', json.dumps(dict(before=before,after=after),indent=2))
            for key in ('drop','rx_error','tx_error'):
                assert after['can0'][key] == before['can0'][key]
            assert after['runtime']['overflow'] == before['runtime']['overflow']
            assert after['ui']['flush'] > before['ui']['flush']
    finally:
        hil.dut.command('meter_settings brightness '+str(original))
        settings_result(hil.dut)

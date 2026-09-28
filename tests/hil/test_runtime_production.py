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


def test_dynamic_periodic_publication(hil):
    """真实 App 设置变更关联首个期限，保留 PCAN 原生时间并同时等待 NVM durable。"""
    import functools
    import operator
    import threading
    from tools.hil.dut import KV, atom
    samples = []
    mutex = threading.Lock()

    def receive(message):
        if message.arbitration_id in (0x3c0, 0x2f0) and not message.is_extended_id:
            with mutex:
                samples.append(dict(id=message.arbitration_id, timestamp=message.timestamp, data=list(message.data)))

    def snapshot():
        with mutex:
            return list(samples)

    original = diagnostics(hil.dut.command('meter storage'))['storage']['brightness']
    reports = []
    hil.bus.notifier.add_listener(receive)
    try:
        with transmitting(hil.bus, hil.dbc, hil.scenario, period_ms=10):
            time.sleep(.8)
            for value in (31, 47, 63):
                assert 'QUEUED' in hil.dut.command('meter_settings brightness '+str(value))
                assert 'APPLIED_OR_QUEUED' in settings_result(hil.dut)
                time.sleep(.25)
                raw = hil.dut.command('meter_exec')
                rows = {}
                for line in raw.splitlines():
                    if line.startswith('periodic_first '):
                        fields = {k: atom(v) for k,v in KV.findall(line)}
                        rows.setdefault(fields['entry'], {}).update(fields)
                row = rows[0]
                assert row['success'] == 1 and (row['data0'] | row['data1'] << 8) == value, raw
                # 这是首个计划期限的契约，不是新引入的毫秒 jitter 容差。
                assert 0 <= (row['deadline_ms']-row['acquired_ms']) % (1 << 32) < 50, row
                candidates = [r for r in snapshot() if r['id'] == 0x3c0 and len(r['data']) == 8 and
                              r['data'][0] == value and r['data'][1] == 0 and
                              (r['data'][4] | r['data'][5] << 8) == row['revision'] % 65536]
                assert candidates and candidates[0]['data'][3] == row['data3'], (row, candidates)
                reports.append(dict(first=row, first_wire_native_timestamp=candidates[0]['timestamp'], runtime=raw))
            deadline = time.monotonic() + 12
            while True:
                storage = diagnostics(hil.dut.command('meter storage'))['storage']
                if storage['brightness'] == 63 and not storage['dirty'] and storage['durable_revision'] == storage['ram_revision']:
                    break
                assert time.monotonic() < deadline, storage
                time.sleep(.2)
            fresh_frames = [r for r in snapshot() if r['id'] == 0x2f0]
            assert fresh_frames and fresh_frames[-1]['data'][2] == 1
        # 停止输入后 App 继续 publish；不能因此刷新车速 sample freshness。
        time.sleep(.9)
        tail = [r for r in snapshot() if r['id'] == 0x2f0][-3:]
        assert len(tail) == 3 and all(r['data'][2] == 0 for r in tail), tail
        with transmitting(hil.bus, hil.dbc, hil.scenario, period_ms=10):
            time.sleep(.4)
            assert [r for r in snapshot() if r['id'] == 0x2f0][-1]['data'][2] == 1
        for frame in snapshot():
            data = frame['data']
            assert len(data) == 8 and data[6] == (data[0] ^ 255), frame
            assert functools.reduce(operator.xor, data) == 0, frame
    finally:
        hil.evidence.text('dynamic-periodic.json', json.dumps(dict(changes=reports, frames=snapshot()), indent=2))
        hil.dut.command('meter_settings brightness '+str(original))
        settings_result(hil.dut)
        hil.bus.notifier.remove_listener(receive)

"""pytest HIL 入口：缺硬件默认跳过，失败自动抓取证据。"""
from pathlib import Path
import hashlib
import json
import subprocess
import pytest
from tools.hil.evidence import Evidence

ROOT = Path(__file__).resolve().parents[1]


def pytest_addoption(parser):
    group = parser.getgroup('hil')
    group.addoption('--hil', action='store_true', help='enable physical Demo HIL')
    group.addoption('--can-interface', default='pcan')
    group.addoption('--can-channel', default='PCAN_USBBUS1')
    group.addoption('--can-bitrate', type=int, default=500000)
    group.addoption('--dut-port', default=None)
    group.addoption('--dut-baud', type=int, default=115200)
    group.addoption('--dut-board', default='reference-board', help='expected public board identity')
    group.addoption('--hil-evidence', default='evidence/hil')
    group.addoption('--hil-image', default=None, help='optional flashed image, record SHA256 only')


def pytest_configure(config):
    config.hil_evidence = None
    config.hil_dut = None
    if config.getoption('--hil'):
        if getattr(config.option, 'numprocesses', None):
            raise pytest.UsageError('physical HIL requires a single runner, without xdist')
        config.hil_evidence = Evidence(config.getoption('--hil-evidence'))
        if not config.option.xmlpath:
            config.option.xmlpath = str(config.hil_evidence.path/'junit.xml')
        else:
            config.hil_evidence.metadata['junit_xml'] = str(config.option.xmlpath)


def pytest_collection_modifyitems(config, items):
    if not config.getoption('--hil'):
        for item in items:
            if 'hil' in item.keywords:
                item.add_marker(pytest.mark.skip(reason='physical HIL requires --hil'))


@pytest.hookimpl(hookwrapper=True)
def pytest_runtest_makereport(item, call):
    report = (yield).get_result()
    evidence = item.config.hil_evidence
    if evidence and 'hil' in item.keywords:
        evidence.metadata['tests'].append(dict(name=item.nodeid, phase=report.when, outcome=report.outcome))
        if report.failed and item.config.hil_dut:
            prefix = 'failure-'+hashlib.sha256(item.nodeid.encode()).hexdigest()[:10]+'-'+report.when
            evidence.capture(item.config.hil_dut, prefix)
        evidence.save()


def pytest_sessionfinish(session, exitstatus):
    evidence = session.config.hil_evidence
    if evidence:
        rows = evidence.metadata['tests']
        calls = [r for r in rows if r['phase'] == 'call']
        failed = any(r['outcome'] == 'failed' for r in rows)
        skipped = any(r['outcome'] == 'skipped' for r in rows)
        evidence.metadata['status'] = 'HIL_FAIL' if failed else 'HIL_PASS' if calls and not skipped and exitstatus == 0 else 'HIL_NOT_RUN'
        evidence.metadata['pytest_exitstatus'] = int(exitstatus)
        evidence.metadata['coverage'] = {f'HIL-{i:02d}': any(f'test_hil{i:02d}_' in r['name'] and r['phase'] == 'call' and r['outcome'] == 'passed' for r in rows) for i in range(1, 7)}
        evidence.save()


@pytest.fixture(scope='session')
def hil(request):
    """先确认 DUT 身份，再打开 CAN；初始化失败仍留下证据。"""
    import can
    import serial
    from filelock import Timeout
    from tools.hil.can_bus import CanBus
    from tools.hil.dbc import Dbc
    from tools.hil.dut import DutSession, info, diagnostics
    from tools.hil.scenario import load
    from types import SimpleNamespace
    config, dut, bus = request.config, None, None
    if not config.getoption('--hil'): pytest.skip('requires --hil')
    evidence = config.hil_evidence
    interface = config.getoption('--can-interface')
    evidence.metadata.update(can_interface=interface, can_channel=config.getoption('--can-channel'), bitrate=config.getoption('--can-bitrate'), uart_port=config.getoption('--dut-port'))
    evidence.save()
    if interface == 'virtual': pytest.skip('virtual CAN is HOST only, never physical HIL')
    if not config.getoption('--dut-port'): pytest.skip('UART port missing: use --dut-port')
    dbc = Dbc(ROOT/'products/demo/protocol/can/demo.dbc')
    scenario = load(ROOT/'tests/hil/scenarios/normal.yaml', dbc)
    evidence.metadata.update(dbc_sha256=dbc.sha256, scenario=scenario,
        host_platform_sha=subprocess.check_output(['git', '-C', str(ROOT), 'rev-parse', 'HEAD'], text=True).strip())
    image = config.getoption('--hil-image')
    evidence.metadata['image_sha256'] = hashlib.sha256(Path(image).read_bytes()).hexdigest() if image else 'NOT_PROVIDED'
    evidence.save()
    try:
        try:
            dut = DutSession(config.getoption('--dut-port'), evidence.path/'serial.log', config.getoption('--dut-baud'))
        except (serial.SerialException, OSError, Timeout) as exc:
            evidence.metadata['unavailable_reason'] = str(exc)
            evidence.save()
            pytest.skip('UART unavailable: '+str(exc))
        config.hil_dut = dut
        raw = dut.command('meter info')
        evidence.text('meter-info.txt', raw)
        identity = info(raw)
        evidence.metadata.update(platform_sha=identity.get('platform'), sdk_identity=identity.get('sdk'), lvgl_aic_revision=identity.get('lvgl-aic'), board=identity.get('board'), dut_identity=identity)
        evidence.save()
        assert identity['product'] == 'reference-demo', 'wrong DUT product'
        assert identity['board'] == config.getoption('--dut-board'), 'wrong DUT board'
        before = dut.command('meter diag')
        evidence.text('diag-before.txt', before)
        state = diagnostics(before)
        assert state['can0']['open'] == 1 and state['can0']['bitrate'] == config.getoption('--can-bitrate')
        try:
            bus = CanBus(interface, config.getoption('--can-channel'), config.getoption('--can-bitrate'), evidence.path/'can.asc')
        except (can.CanError, OSError, ImportError) as exc:
            evidence.metadata['unavailable_reason'] = str(exc)
            evidence.save()
            pytest.skip('CAN backend unavailable: '+str(exc))
        yield SimpleNamespace(dut=dut, bus=bus, dbc=dbc, scenario=scenario, evidence=evidence)
    except BaseException:
        if dut: evidence.capture(dut, 'setup-or-session-failure')
        raise
    finally:
        try:
            if bus:
                evidence.metadata['host_can_counters'] = bus.counters()
                bus.close()
        finally:
            if dut:
                try:
                    evidence.text('diag-after.txt', dut.command('meter diag'))
                    evidence.text('trace.txt', dut.trace_dump())
                except Exception:
                    evidence.capture(dut, 'teardown-failure')
                    raise
                finally:
                    dut.close()
                    config.hil_dut = None
                    evidence.metadata['uart_released'] = True
                    evidence.metadata['can_released'] = bus is not None
                    evidence.save()

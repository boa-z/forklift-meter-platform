"""仅解析数据描述，不实现表达式、循环或业务状态机 DSL。"""
from contextlib import contextmanager
import math
from pathlib import Path
import yaml


def load(path, dbc):
    """拒绝未知字段、重复消息和非有限时长，编码校验信号完整性。"""
    data = yaml.safe_load(Path(path).read_text(encoding='utf-8'))
    if not isinstance(data, dict) or set(data) != {'name', 'period_ms', 'messages'}:
        raise ValueError('scenario requires name, period_ms, messages only')
    if not isinstance(data['name'], str) or not data['name']:
        raise ValueError('scenario name required')
    period = data['period_ms']
    if isinstance(period, bool) or not isinstance(period, (int, float)) or not math.isfinite(period) or period <= 0:
        raise ValueError('positive finite period_ms required')
    if not isinstance(data['messages'], list) or not data['messages']:
        raise ValueError('nonempty messages required')
    names = []
    for row in data['messages']:
        if not isinstance(row, dict) or set(row) != {'message', 'signals'} or not isinstance(row['signals'], dict):
            raise ValueError('each message requires message and signals only')
        dbc.encode(row['message'], row['signals'])
        names.append(row['message'])
    if len(names) != len(set(names)): raise ValueError('duplicate message')
    return data


@contextmanager
def transmitting(bus, dbc, scenario, overrides=None, period_ms=None):
    """使用 python-can 周期任务，退出时确保停止全部本场景任务。"""
    tasks = []
    try:
        for row in scenario['messages']:
            values = row['signals'] | (overrides or {}).get(row['message'], {})
            tasks.append(bus.periodic(dbc.encode(row['message'], values), (period_ms or scenario['period_ms'])/1000))
        yield
        if bus.notifier.exception:
            raise RuntimeError('CAN receive thread failed') from bus.notifier.exception
        for task in tasks:
            thread = getattr(task, 'thread', None)
            if thread is not None and not thread.is_alive():
                raise RuntimeError('CAN periodic sender stopped unexpectedly')
    finally:
        bus.stop_tasks(tasks)

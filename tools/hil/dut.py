"""基于 pySerial 的单所有者 DUT 会话和稳定诊断字段解析。"""
import hashlib
import json
import math
import os
from pathlib import Path
import re
import tempfile
import threading
import time

from filelock import FileLock
import serial
from tools.serial.meter_serial import write_command

ANSI = re.compile(r'\x1b\[[0-?]*[ -/]*[@-~]')
KV = re.compile(r'([A-Za-z_][\w.-]*)=([^\s]*)')


def clean(text):
    """去除显示控制序列，原始字节仍单独保存。"""
    return ANSI.sub('', text).replace('\r', '')


def atom(value):
    """保留不可用标记，仅转换确定的整数与有限浮点数。"""
    try:
        return int(value, 16) if value.lower().startswith('0x') else int(value)
    except ValueError:
        try:
            number = float(value)
            return number if math.isfinite(number) else value
        except ValueError:
            return value


def diagnostics(text):
    """按模块聚合 key=value；忽略提示符、回显和异步日志。"""
    result = {}
    for line in clean(text).splitlines():
        match = re.match(r'^\s*(runtime|can\d+|domain|pdo|sdo|touch|ui|storage|trace)\s+(.*)$', line)
        if match:
            key, tail = match.groups()
            result.setdefault(key, {}).update({k: atom(v) for k, v in KV.findall(tail)})
            if tail.startswith('unavailable'):
                result[key]['available'] = False
    return result


def info(text):
    """读取构建身份，不把命令回显当作响应。"""
    return {k: v.strip() for k, v in re.findall(r'^[ \t]*([\w-]+)[ \t]*:[ \t]*([^\n]+)', clean(text), re.M) if v.strip()}


def signal(text, key):
    """信号必须包含两行完整字段、匹配 canonical key 和有限数值。"""
    lines = clean(text).splitlines()
    result = {}
    for line in lines:
        if re.match(r'^\s*(signal id=|state=)', line):
            result.update({k: atom(v) for k, v in KV.findall(line)})
    required = {'id', 'key', 'value', 'state', 'source', 'timestamp_ms', 'age_ms', 'stale_ms'}
    if not required <= result.keys() or result['key'] != key:
        raise ValueError(f'incomplete or mismatched signal response: {key}')
    if result['state'] not in {'UNKNOWN', 'VALID', 'STALE', 'ERROR'} or not isinstance(result['value'], (int, float)):
        raise ValueError('invalid signal state/value')
    return result


def trace(text):
    """按稳定数值字段解析 Trace，检查记录数量完整。"""
    header = diagnostics(text).get('trace', {})
    records = []
    for line in clean(text).splitlines():
        match = re.match(r'^\s*(\d+)\s+(\w+)\s+(\w+)\s+module=(\d+)\s+event=(\d+)\s+arg0=(\d+)\s+arg1=(0x[\da-fA-F]+)', line)
        if match:
            t, module_name, event_name, module, event, a, b = match.groups()
            records.append(dict(timestamp_ms=int(t), module_name=module_name, event_name=event_name,
                                module=int(module), event=int(event), arg0=int(a), arg1=int(b, 16)))
    if header.get('entries') != len(records):
        raise ValueError('incomplete trace response')
    return records


def driver_status(text):
    """解析 RT-Thread 原生 canstat，区别驱动计数与 Runtime 边界计数。"""
    result = {}
    for key, label in [('rx', 'Total.receive.packages'), ('drop', 'Dropped.receive.packages'),
                       ('tx', 'Total..send...packages'), ('tx_drop', 'Dropped...send..packages')]:
        match = re.search(re.escape(label)+r':\s*(\d+)', clean(text))
        if match: result[key] = int(match.group(1))
    if len(result) != 4: raise ValueError('incomplete native canstat response')
    return result


def complete(command, text):
    """用最后一项稳定字段判断完整性，不依赖特定提示符。"""
    # UART 可在字段中间分片；仅以已经收到换行的完整行参与判定。
    text = text[:text.rfind('\n')+1]
    try:
        if command == 'meter_exec stop':
            return 'runtime stop=QUEUED' in clean(text).splitlines()
        if command.startswith('meter_update maintenance '):
            return 'maintenance requested; Product owns admission' in clean(text).splitlines()
        if command == 'meter_update info':
            for row in clean(text).splitlines():
                if row.startswith('{'):
                    try:
                        return {'state', 'received', 'total', 'maintenance'} <= json.loads(row).keys()
                    except (ValueError, TypeError):
                        pass
            return False
        if command == 'meter_exec':
            return bool(re.search(r'^shutdown wait_ms=\d+ overdue=[01];', clean(text), re.M))
        if command.startswith('meter_settings '):
            if command.endswith(' result'):
                return bool(re.search(r'^settings result=(?:PENDING_OR_NONE|(?:APPLIED_OR_QUEUED|REJECTED).*durability=query-meter-storage)$', clean(text), re.M))
            return bool(re.search(r'^settings (result=QUEUED;|REJECTED|BUSY)', clean(text), re.M))
        if command.startswith('canstat '):
            driver_status(text)
            return True
        if command == 'meter info':
            fields = info(text)
            return {'product', 'platform', 'sdk', 'lvgl-aic', 'board', 'uptime_ms'} <= fields.keys() and fields['uptime_ms'].isdigit()
        if command.startswith('meter signal '):
            signal(text, command.split()[-1])
            return True
        if command == 'meter trace dump':
            trace(text)
            return True
        if command == 'meter trace clear':
            return diagnostics(text).get('trace', {}).get('result') == 'OK'
        data = diagnostics(text)
        if command in ('meter diag', 'meter storage'):
            row = data.get('storage', {})
            storage_complete = row.get('available') is False or {'state', 'durable_revision', 'imperial'} <= row.keys()
            return storage_complete and (command == 'meter storage' or 'error' in data.get('domain', {}))
        if command == 'meter runtime': return 'resets' in data.get('runtime', {})
        if command == 'meter domain': return 'error' in data.get('domain', {})
        if command == 'meter trace': return 'overwritten' in data.get('trace', {})
        if command.startswith('meter can'):
            bus = command.split()[2] if len(command.split()) == 3 else '1'
            row = data.get('can'+bus, {})
            return row.get('available') is False or 'last_tx_age_ms' in row
    except (ValueError, KeyError):
        pass
    return False


class DutSession:
    """持续保存原始 UART；跨进程锁和系统独占限制一个 runner 所有者。"""
    def __init__(self, port, log, baud=115200, timeout=5.0):
        if timeout <= 0 or not math.isfinite(timeout):
            raise ValueError('timeout must be positive and finite')
        self.timeout = timeout
        canonical = port.upper().removeprefix('\\\\.\\') if os.name == 'nt' else os.path.realpath(port)
        digest = hashlib.sha256(canonical.encode()).hexdigest()
        self.lock = FileLock(str(Path(tempfile.gettempdir()) / ('meter-hil-'+digest+'.lock')))
        self.lock.acquire(timeout=0)
        self.raw = None
        self.port = None
        try:
            kwargs = dict(baudrate=baud, timeout=.05, write_timeout=2, do_not_open=True)
            if os.name != 'nt': kwargs['exclusive'] = True
            self.port = serial.serial_for_url(port, **kwargs)
            self.port.dtr = self.port.rts = False
            self.port.open()
            self.raw = open(log, 'ab')
        except BaseException:
            if self.port: self.port.close()
            self.lock.release()
            raise
        self.condition = threading.Condition()
        self.command_lock = threading.Lock()
        self.buffer = bytearray()
        self.collecting = False
        self.stop = threading.Event()
        self.error = None
        self.reader = threading.Thread(target=self._read, daemon=True)
        self.reader.start()

    def _read(self):
        """唯一读取者持续写原始日志，命令期间另存有界响应。"""
        try:
            while not self.stop.is_set():
                data = self.port.read(min(max(self.port.in_waiting, 1), 4096))
                if data:
                    self.raw.write(data)
                    self.raw.flush()
                    with self.condition:
                        if self.collecting:
                            self.buffer.extend(data)
                            if len(self.buffer) > 1048576:
                                raise RuntimeError('UART response exceeds 1 MiB')
                        self.condition.notify_all()
        except BaseException as exc:
            with self.condition:
                self.error = exc
                self.condition.notify_all()

    def command(self, text, timeout=None):
        """有限等待完整响应；回显、日志或部分结果不能造成假成功。"""
        deadline = time.monotonic() + (self.timeout if timeout is None else timeout)
        with self.command_lock:
            with self.condition:
                self.buffer.clear()
                self.collecting = True
                try:
                    write_command(self.port, text)
                    while True:
                        if self.error: raise OSError('UART reader failed') from self.error
                        response = self.buffer.decode('utf-8', errors='replace')
                        if complete(text, response):
                            return response
                        remaining = deadline - time.monotonic()
                        if remaining <= 0:
                            raise TimeoutError(f'incomplete response to {text}')
                        self.condition.wait(min(.05, remaining))
                finally:
                    self.collecting = False

    def signal(self, key):
        """查询指定 Domain 信号。"""
        return signal(self.command('meter signal '+key), key)

    def trace_dump(self):
        """返回完整 Trace 文本。"""
        return self.command('meter trace dump', timeout=10)

    def close(self):
        """停止读取并释放串口与跨进程所有权。"""
        self.stop.set()
        self.reader.join(timeout=2)
        self.port.close()
        self.raw.close()
        self.lock.release()

#!/usr/bin/env python3
"""使用 pyserial 保存 UART 原始字节并发送原生 MSH 命令；不解释车辆协议。"""
import argparse
import sys
import time
from pathlib import Path


def capture(port, output, duration=None, clock=time.monotonic):
    """持续采集；duration=None 时由 Ctrl-C 结束，已读字节立即落到输出流。"""
    start = clock()
    count = 0
    while duration is None or clock() - start < duration:
        data = port.read(min(max(port.in_waiting, 1), 4096))
        if data:
            output.write(data)
            output.flush()
            count += len(data)
    return count


def write_command(port, text):
    """校验并发送一条 MSH 命令，供同步助手与 HIL 会话复用。"""
    if not text.strip() or any(ord(c) < 32 or ord(c) == 127 for c in text):
        raise ValueError('command must be one nonempty line without control characters')
    payload = text.encode('utf-8') + b'\r\n'
    sent = port.write(payload)
    if sent != len(payload):
        raise OSError('incomplete serial command write')
    port.flush()


def command(port, text, output, timeout=5.0, idle=0.5, clock=time.monotonic):
    """发送单行命令并收集原始响应；收到字节后等待 idle，最多等待 timeout。"""
    write_command(port, text)
    start = clock()
    last = None
    count = 0
    while clock() - start < timeout:
        data = port.read(min(max(port.in_waiting, 1), 4096))
        now = clock()
        if data:
            output.write(data)
            output.flush()
            count += len(data)
            last = now
        elif last is not None and now - last >= idle:
            break
    return count


class Tee:
    """同时保存文件和显示 stdout，保持所有字节与原顺序。"""
    def __init__(self, *outputs):
        self.outputs = outputs

    def write(self, data):
        for output in self.outputs:
            output.write(data)

    def flush(self):
        for output in self.outputs:
            output.flush()


def positive(value):
    result = float(value)
    if not 0 < result < float('inf'):
        raise argparse.ArgumentTypeError('must be finite and positive')
    return result


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--port', required=True, help='COMx or /dev/ttyUSBx; pyserial URLs also supported')
    parser.add_argument('--baud', type=int, default=115200)
    commands = parser.add_subparsers(dest='mode', required=True)
    cap = commands.add_parser('capture', help='append raw serial bytes until Ctrl-C')
    cap.add_argument('output', type=Path)
    cap.add_argument('--duration', type=positive)
    cmd = commands.add_parser('command', help='send one MSH line; raw response to stdout')
    cmd.add_argument('text')
    cmd.add_argument('--output', type=Path, help='also append raw response to this file')
    cmd.add_argument('--timeout', type=positive, default=5.0)
    cmd.add_argument('--idle', type=positive, default=0.5)
    args = parser.parse_args(argv)
    if args.baud <= 0:
        parser.error('--baud must be positive')
    if args.mode == 'command' and args.idle > args.timeout:
        parser.error('--idle must not exceed --timeout')
    try:
        import serial
    except ImportError:
        parser.exit(2, 'Install host dependency: python -m pip install -r tools/serial/requirements.txt\n')
    try:
        # 先配置控制线，再打开设备；不清空输入，保留已有启动日志。
        port = serial.serial_for_url(args.port, baudrate=args.baud, timeout=0.1,
                                     write_timeout=2, do_not_open=True)
        port.dtr = False
        port.rts = False
        with port:
            target = args.output
            if target:
                target.parent.mkdir(parents=True, exist_ok=True)
                with target.open('ab') as output:
                    if args.mode == 'capture':
                        capture(port, output, args.duration)
                    else:
                        if command(port, args.text, Tee(output, sys.stdout.buffer), args.timeout, args.idle) == 0:
                            raise TimeoutError('no serial response before timeout')
            elif command(port, args.text, sys.stdout.buffer, args.timeout, args.idle) == 0:
                raise TimeoutError('no serial response before timeout')
    except KeyboardInterrupt:
        return 0
    except (OSError, ValueError, serial.SerialException) as error:
        print(str(error), file=sys.stderr)
        return 2
    return 0


if __name__ == '__main__':
    raise SystemExit(main())

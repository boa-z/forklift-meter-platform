#!/usr/bin/env python3
"""Host 捕获使用 python-can 后端及原生日志 Writer；不发送车辆命令。"""
import argparse
import time
import can

p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--interface', default='socketcan')
p.add_argument('--channel', default='can0')
p.add_argument('--seconds', type=float, default=10)
p.add_argument('--output', required=True)
a = p.parse_args()
if a.seconds <= 0: p.error('--seconds must be positive')
with can.Bus(interface=a.interface, channel=a.channel) as bus, can.Logger(a.output) as writer:
    end = time.monotonic() + a.seconds
    while time.monotonic() < end:
        msg = bus.recv(timeout=min(0.2, max(0, end-time.monotonic())))
        if msg is not None: writer(msg)

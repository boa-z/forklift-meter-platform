#!/usr/bin/env python3
"""使用 python-can 读取 candump/ASC/BLF，再调用真实 C 协议及 Domain。"""
import argparse
import json
from pathlib import Path
import subprocess
import can


def replay(runner, log, channel_map=None, settle_ms=0):
    channel_map = channel_map or {'can0': 0, 'can1': 1, '0': 0, '1': 1}
    lines = []
    start = previous = None
    now = 0
    with can.LogReader(str(log)) as reader:
        for msg in reader:
            if msg.is_error_frame or msg.is_remote_frame or msg.is_fd or len(msg.data) != msg.dlc:
                raise ValueError('Only classic CAN data frames are supported')
            if start is None:
                start = msg.timestamp
            if previous is not None and msg.timestamp < previous:
                raise ValueError('Log timestamps must be monotonic')
            previous = msg.timestamp
            now = round((msg.timestamp - start) * 1000)
            if not 0 <= now <= 0xffffffff - settle_ms:
                raise ValueError('Replay exceeds uint32 millisecond range')
            channel = str(msg.channel)
            if channel not in channel_map:
                raise ValueError(f'Unmapped channel: {channel}; specify --channel-map')
            lines.append(f'F {now} {channel_map[channel]} {msg.arbitration_id:x} {int(msg.is_extended_id)} {msg.dlc} {msg.data.hex() or "-"}')
    if start is None:
        raise ValueError('Empty CAN log')
    lines.append(f'T {now + settle_ms}')
    result = subprocess.run([str(runner)], input='\n'.join(lines)+'\n', text=True, capture_output=True, check=True)
    return json.loads(result.stdout)


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('log', type=Path)
    p.add_argument('--runner', type=Path, required=True)
    p.add_argument('--settle-ms', type=int, default=0)
    p.add_argument('--channel-map', help='JSON mapping, e.g. {"vcan0":0,"vcan1":1}')
    p.add_argument('--output', type=Path)
    a = p.parse_args()
    if a.settle_ms < 0: p.error('--settle-ms must be nonnegative')
    mapping = json.loads(a.channel_map) if a.channel_map else None
    if mapping and any(type(v) is not int or v not in (0, 1) for v in mapping.values()):
        p.error('Channel destinations must be CAN0=0 or CAN1=1')
    result = json.dumps(replay(a.runner.resolve(), a.log, mapping, a.settle_ms), ensure_ascii=False, indent=2) + '\n'
    if a.output: a.output.write_text(result, encoding='utf-8')
    else: print(result, end='')

if __name__ == '__main__': main()

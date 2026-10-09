#!/usr/bin/env python3
"""把 Domain path YAML 转为更新流；路径解析来自实际产品 runner 的目录。"""
import argparse
import json
import math
import os
from pathlib import Path
import subprocess
import tempfile
import yaml

STATES = {'unknown': 0, 'valid': 1, 'stale': 2, 'error': 3}

def updates(fixture, runner):
    catalog = json.loads(subprocess.check_output([str(runner), '--catalog'], text=True))['signals']
    data = yaml.safe_load(Path(fixture).read_text(encoding='utf-8'))
    if not isinstance(data, dict) or not data: raise ValueError('Fixture must contain Domain paths')
    lines = []
    for path, row in data.items():
        if path not in catalog: raise ValueError(f'Unknown Domain path: {path}')
        if not isinstance(row, dict) or set(row) != {'value', 'state'}: raise ValueError(f'Invalid fixture row: {path}')
        value = float(row['value'])
        if not math.isfinite(value) or row['state'] not in STATES: raise ValueError(f'Invalid value/state: {path}')
        lines.append(f"U {catalog[path]['id']} {value:.9g} {STATES[row['state']]} 0 2")
    return '\n'.join(lines) + '\n'

def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('fixture', type=Path)
    p.add_argument('--runner', type=Path, required=True)
    p.add_argument('--simulator', type=Path, required=True)
    p.add_argument('--capture', type=Path, required=True)
    p.add_argument('--language', choices=['english','chinese'], default='english')
    a = p.parse_args()
    stream = updates(a.fixture, a.runner.resolve())
    # 在启动 UI 前，先让同一产品的 C core 验证整个更新流。
    subprocess.run([str(a.runner.resolve())], input=stream, text=True, check=True, capture_output=True)
    with tempfile.TemporaryDirectory() as tmp:
        path = Path(tmp) / 'fixture.updates'
        path.write_text(stream, encoding='utf-8')
        subprocess.run([str(a.simulator.resolve()), '--fixture', str(path), '--hidden', '--frames', '30',
                        '--capture', str(a.capture.resolve()), '--set-language', a.language],
                       env={**os.environ, 'SDL_VIDEODRIVER':'dummy'}, check=True)

if __name__ == '__main__': main()

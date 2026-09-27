#!/usr/bin/env python3
"""验证新增第二产品期间五个公共目录保持基线内容，不依赖 Git checkout 深度。"""
from pathlib import Path
import hashlib,json
ROOT=Path(__file__).resolve().parents[1]
baseline=json.loads((ROOT/'examples/reference-b/common-baseline.json').read_text())
actual={}
for area in baseline['areas']:
    for p in sorted((ROOT/area).rglob('*')):
        if p.is_file():
            actual[p.relative_to(ROOT).as_posix()]=hashlib.sha256(p.read_bytes().replace(b'\r\n',b'\n')).hexdigest()
assert actual==baseline['files'],'Common boundary changed; review and deliberately renew the architecture baseline'
print('Reference-B protected common directories: zero changes PASS')

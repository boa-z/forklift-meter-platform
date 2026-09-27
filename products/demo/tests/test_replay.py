"""跨日志格式检查相同 C 解码结果，避免只验证 Python 编解码自洽。"""
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import can
ROOT = Path(os.environ['METER_PLATFORM_ROOT'])
PRODUCT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools/protocol'))
from replay import replay
runner = Path(sys.argv[1]).resolve()
log = PRODUCT / 'fixtures/can/normal.log'
expected = replay(runner, log)
assert expected['dispatched'] == 5 and expected['decode_failed'] == 0
assert expected['signals']['vehicle.speed']['value'] == 25
assert expected['signals']['vehicle.steering']['value'] == -45
assert expected['signals']['energy.soc']['value'] == 78
assert expected['signals']['lift.load']['value'] == 850
assert expected['signals']['vehicle.speed']['source'] == 1
stale = replay(runner, log, settle_ms=800)
assert stale['signals']['vehicle.speed']['state'] == 'stale'
assert stale['signals']['energy.soc']['state'] == 'valid'
with tempfile.TemporaryDirectory() as tmp:
    for ext in ('asc', 'blf'):
        path = Path(tmp) / ('synthetic.'+ext)
        with can.LogReader(str(log)) as reader, can.Logger(str(path)) as writer:
            for msg in reader:
                msg.channel = 0
                writer(msg)
        actual = replay(runner, path)
        assert actual == expected, (ext, actual)
for stream in ('F 0 0 100 0 9 000000000000000000\n', 'F 0 0 100 0 1 xx\n', 'U 65536 1 1 0 1\n'):
    assert subprocess.run([str(runner)], input=stream, text=True, capture_output=True).returncode != 0
print('CAN replay candump/ASC/BLF and stale semantics PASS')

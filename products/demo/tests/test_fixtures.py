"""验证 YAML 到 C core，再到双语 UI 截图的数据路径。"""
import json
import os
import os
from pathlib import Path
import subprocess
import sys
import tempfile
ROOT = Path(os.environ['METER_PLATFORM_ROOT'])
PRODUCT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools/fixtures'))
from run_ui import updates
runner, simulator = [Path(p).resolve() for p in sys.argv[1:3]]
with tempfile.TemporaryDirectory() as tmp:
    for name, state, speed in [('normal','valid',12.5), ('warning','valid',8), ('stale','stale',12.5), ('error','error',0)]:
        stream = updates(PRODUCT / f'fixtures/ui/{name}.yaml', runner)
        result = json.loads(subprocess.check_output([str(runner)], input=stream, text=True))
        assert result['signals']['vehicle.speed']['state'] == state
        assert result['signals']['vehicle.speed']['value'] == speed
        path = Path(tmp) / 'input.updates'; path.write_text(stream)
        for language in ('english','chinese'):
            capture = Path(tmp) / (name+language+'.bmp')
            report = json.loads(subprocess.check_output([str(simulator),'--fixture',str(path),'--hidden','--frames','30',
                '--set-language',language,'--capture',str(capture)], text=True, env={**os.environ,'SDL_VIDEODRIVER':'dummy'}))
            assert report['result'] == 'PASS' and report['dispatched'] == 0
            assert report['speed'] == speed and report['speed_state'] == {'valid':1,'stale':2,'error':3}[state]
            assert capture.read_bytes()[:2] == b'BM' and capture.stat().st_size > 10000
print('Domain fixtures normal/warning/stale/error and bilingual screenshot smoke PASS')

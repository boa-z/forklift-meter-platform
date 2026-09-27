"""断言 Reference-B 独立路由、缩放、状态与不同 stale 时间。"""
import json, os, subprocess, sys, tempfile
from pathlib import Path
ROOT=Path(os.environ['METER_PLATFORM_ROOT']); PRODUCT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'tools/protocol'))
from replay import replay
runner=Path(sys.argv[1]).resolve()
r=replay(runner,PRODUCT/'fixtures/can/normal.log')
s=r['signals']
assert len(s)==4 and r['dispatched']==2
assert s['drive.velocity']['id']==701 and s['drive.velocity']['value']==6.5
assert s['drive.torque']['value']==-15 and s['battery.remaining']['value']==82
assert s['environment.temperature']['value']==23 and s['drive.velocity']['source']==19
r=replay(runner,PRODUCT/'fixtures/can/normal.log',settle_ms=700)
assert r['signals']['drive.velocity']['state']=='stale'
assert r['signals']['drive.torque']['state']=='stale'
assert r['signals']['battery.remaining']['state']=='valid'
bad=subprocess.run([str(runner)],input='F 0 1 18ff5201 1 1 00\n',text=True,capture_output=True)
assert bad.returncode!=0
wrong=json.loads(subprocess.check_output([str(runner)],input='F 0 0 18ff5201 1 8 4100e2ff00000000\n',text=True))
assert wrong['unrouted']==1 and wrong['signals']['drive.velocity']['state']=='unknown'
print('Reference-B CAN1 extended routing, scaling and freshness PASS')

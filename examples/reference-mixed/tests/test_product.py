"""断言 Reference-Mixed CAN0 DBC 与 CAN1 PDO 同时进 Domain，且 stale 按各自超时。"""
import json, os, subprocess, sys
from pathlib import Path
ROOT=Path(os.environ['METER_PLATFORM_ROOT']); PRODUCT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'tools/protocol'))
from replay import replay
runner=Path(sys.argv[1]).resolve()
r=replay(runner,PRODUCT/'fixtures/can/normal.log')
s=r['signals']
assert r['dispatched']==3, r
assert s['mixed.can0.speed']['value']==12.5, s
assert s['mixed.can0.soc']['value']==82, s
assert s['mixed.pdo.speed']['value']==6.5, s
assert s['mixed.pdo.torque']['value']==-15, s
# PDO 500ms 超时，CAN0 speed 750ms：settle 600ms 时 PDO stale 而 CAN0 仍 valid。
r=replay(runner,PRODUCT/'fixtures/can/normal.log',settle_ms=600)
assert r['signals']['mixed.pdo.speed']['state']=='stale', r
assert r['signals']['mixed.can0.speed']['state']=='valid', r
print('Reference-Mixed CAN0 DBC + CAN1 PDO routing and freshness PASS')

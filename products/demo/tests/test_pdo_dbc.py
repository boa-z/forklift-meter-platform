"""以真实 C 编码输出校验独立发送 DBC，防止工具元数据与固件漂移。"""
from pathlib import Path
import subprocess
import sys
import cantools

db = cantools.database.load_file(Path(__file__).resolve().parents[1] / 'protocol/can/demo_tx.dbc')
vectors = dict(line.split() for line in subprocess.check_output([sys.argv[1]], text=True).splitlines())
motion = db.get_message_by_name('demo_tpdo_motion')
status = db.get_message_by_name('demo_tpdo_status')
assert (motion.frame_id, status.frame_id) == (0x381, 0x481)
assert motion.cycle_time == status.cycle_time == 100
assert motion.senders == status.senders == ['Meter']
assert motion.length == status.length == 8
assert not motion.is_extended_frame and not status.is_extended_frame
decoded = motion.decode(bytes.fromhex(vectors['motion']))
assert decoded == dict(speed=12.5, height=1.25, load=780, soc=75, fresh=1, sequence=127)
decoded = status.decode(bytes.fromhex(vectors['status']))
assert decoded == dict(seat=1, brake=0, neutral=1, charging=0, warning=1, fresh=1,
                       sequence=255, layout_version=1, generation=0x12345678)
for name, definition in [('stale_motion', motion), ('stale_status', status), ('zero_motion', motion)]:
    decoded = definition.decode(bytes.fromhex(vectors[name]))
    assert decoded['fresh'] == (1 if name == 'zero_motion' else 0)
    assert definition.encode(decoded) == bytes.fromhex(vectors[name])
print('Demo PDO C payload / DBC PASS')

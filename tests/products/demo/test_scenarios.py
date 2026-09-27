import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
exe = Path(sys.argv[1]).resolve()
env = dict(os.environ, SDL_VIDEODRIVER="dummy")
def run(*args):
    p = subprocess.run([str(exe), "--hidden", "--frames", "80", *args], env=env, text=True, capture_output=True, timeout=25, check=True)
    report = json.loads(p.stdout.strip().splitlines()[-1])
    assert report["result"] == "PASS", report
    assert report["objects"] == report["objects_final"], report
    assert report["decode_failed"] == report["overflow"] == 0, report
    return report
normal=run("--scenario", "normal"); assert normal["speed_state"]==1 and normal["dispatched"]>0
warning=run("--scenario", "warning"); assert warning["faults"] & 1 and warning["faults"] & (1<<8)
for name in ("stale", "offline"):
    r=run("--scenario",name); assert r["speed_state"]==2 and r["speed"]==25 and r["faults"] & (1<<4),r
assert run("--scenario","error")["faults"] & (1<<3)
assert run("--scenario","unknown")["speed_state"]==0
for language, locale in (("english", "en"), ("chinese", "zh-CN")):
    for page in range(4):
        r = run("--page", str(page), "--set-language", language)
        assert r["page"] == page and r["language"] == locale, r
invalid = subprocess.run([str(exe), "--set-language", "unsupported"], env=env, capture_output=True, timeout=25)
assert invalid.returncode == 2
with tempfile.TemporaryDirectory() as temp:
    settings=str(Path(temp)/"preferences.bin")
    assert run("--settings",settings,"--set-units","imperial")["imperial"]
    assert run("--settings",settings)["imperial"]
    assert run("--settings", settings, "--set-language", "chinese")["language"] == "zh-CN"
    assert run("--settings", settings)["language"] == "zh-CN"
    assert run("--settings", settings, "--set-language", "english")["language"] == "en"
    assert run("--settings", settings)["language"] == "en"
    Path(settings).write_bytes(b"corrupt")
    r = run("--settings",settings)
    assert not r["imperial"] and r["language"] == "en"
print("Demo scenarios, touch navigation and settings restart PASS")

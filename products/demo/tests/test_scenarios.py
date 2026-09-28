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
    assert report["faults"] == len(report["fault_ids"]), report
    return report
# 故障身份与产品目录同源于 schema，所以场景断言指向具体条目，
# 而不是平台某个字里的位序号。
catalog = json.loads((Path(__file__).resolve().parents[1] / "catalog/demo_catalog.json").read_text(encoding="utf-8"))
FAULTS = {row[1]: row[0] for row in catalog["faults"]}
normal=run("--scenario", "normal"); assert normal["speed_state"]==1 and normal["dispatched"]>0
warning=run("--scenario", "warning"); assert {FAULTS["low_charge"], FAULTS["generic_warning"]} <= set(warning["fault_ids"])
for name in ("stale", "offline"):
    r=run("--scenario",name); assert r["speed_state"]==2 and r["speed"]==25 and FAULTS["communication_fault"] in r["fault_ids"],r
assert FAULTS["sensor_fault"] in run("--scenario","error")["fault_ids"]
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
    Path(settings + ".0").write_bytes(b"corrupt")
    Path(settings + ".1").write_bytes(b"corrupt")
    failed = subprocess.run([str(exe), "--hidden", "--frames", "2", "--settings", settings], env=env, capture_output=True, timeout=25)
    assert failed.returncode == 7
    assert Path(settings + ".0").read_bytes() == b"corrupt"
    assert Path(settings + ".1").read_bytes() == b"corrupt"
print("Demo scenarios, touch navigation and settings restart PASS")

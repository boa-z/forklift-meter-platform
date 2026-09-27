#!/usr/bin/env python3
from pathlib import Path
import hashlib
import json
import subprocess
ROOT=Path(__file__).resolve().parents[1]
m=json.loads((ROOT/'assets/manifest.json').read_text(encoding='utf-8'))
for asset in m['icons']:
    assert all(asset.get(k) for k in ('source','upstream_project','upstream_file','commit','license','modifications','sha256')),asset
    assert asset['license']=='MIT' and len(asset['commit'])==40
    p=ROOT/asset['file']; assert hashlib.sha256(p.read_bytes()).hexdigest()==asset['sha256'],p
    svg=p.read_text(); assert 'viewBox="0 0 24 24"' in svg and 'stroke-width="2"' in svg,p
for license in ('Tabler-MIT.txt','Montserrat-OFL.txt','LVGL-MIT.txt'):
    assert (ROOT/'assets/LICENSES'/license).stat().st_size>500,license
assert (ROOT/'generated/demo_icons.c').is_file()
assert m['font']['license']=='OFL-1.1'
head=subprocess.check_output(['git','-C',str(ROOT/'third_party/lvgl'),'rev-parse','HEAD'],text=True).strip()
assert head==m['font']['commit'],'Unreviewed font dependency update'
print('Asset provenance, dimensions, licenses and hashes PASS')

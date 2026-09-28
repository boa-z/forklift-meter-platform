#!/usr/bin/env python3
"""Reject confidential marker fingerprints without publishing the marker list.
Use --extra-denylist for a private, newline-separated audit extension.
String scanning complements the documented source and visual provenance review.
"""
from pathlib import Path
import argparse, hashlib, json, re, subprocess
ROOT=Path(__file__).resolve().parents[1]
FINGERPRINTS={7: ['68db150428098e2e277a302fed0e327818282747a2393a827e3908cbace5e1c6', '201856748f3b6bdf541fb470fcd169f9964cb0779db97520a108ea95166eabdf'], 2: ['6cb27f0b230d390874b586ac4895683237c430cce1aa44335ad6f9b0c4256373'], 9: ['b51a81b34fcac5b8e4f49e1fad31f2b91fef043e647d812c1432f6250fc24a7b', '1afd166f0c565645899d986d39ef613b175d6fd998de372a39b89770bd188878'], 10: ['7748c55bc8b631d3727b00ee9107da06f0beed949a1627627101e9af49d13997'], 4: ['a48f7ec134d51ba3f087068e43ec3f41c6f609c9e2d2dc77d2157e31315a3cf2']}
p=argparse.ArgumentParser(); p.add_argument('--extra-denylist',type=Path); args=p.parse_args()
extra=args.extra_denylist.read_text(encoding='utf-8').casefold().splitlines() if args.extra_denylist else []
paths=subprocess.check_output(['git','-C',str(ROOT),'ls-files','--cached','--others','--exclude-standard','-z']).decode('utf-8').split('\0')
errors=[]; checked=0
for name in sorted(set(paths)):
    f=ROOT/name
    if not name or not f.is_file() or name.startswith('third_party/'): continue
    if f.suffix.lower() in ('.psd','.psb','.pcap','.pcapng','.pem','.key'): errors.append(name+': forbidden asset type'); continue
    raw=f.read_bytes()
    if re.search(rb'-----BEGIN [A-Z ]*PRIVATE KEY-----',raw): errors.append(name+': private key'); continue
    try: text=raw.decode('utf-8').casefold()
    except UnicodeDecodeError: errors.append(name+': unaudited binary'); continue
    checked+=1
    for token in re.findall(r'[a-z0-9_-]+|[\u4e00-\u9fff]+',name.casefold()+' '+text):
        for length, hashes in FINGERPRINTS.items():
            for i in range(len(token)-length+1):
                if hashlib.sha256(token[i:i+length].encode()).hexdigest() in hashes: errors.append(name+': private marker'); break
    if any(t and t in text for t in extra): errors.append(name+': additional private marker')
    if re.search(r'gh[pousr]_[a-z0-9]{30,}|github_pat_[a-z0-9_]{30,}',text): errors.append(name+': credential-shaped token')
if errors: raise SystemExit('\n'.join(sorted(set(errors))))
print(json.dumps({'public_clean':'PASS','files_checked':checked,'scope':'first-party tree; upstream pins reviewed separately'}))

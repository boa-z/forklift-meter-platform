#!/usr/bin/env python3
"""Enforce first-party source dependency directions, not only conventions."""
from pathlib import Path
import json
import re
ROOT = Path(__file__).resolve().parents[1]
RULES = {
    'contracts': ('contracts/',),
    'core': ('contracts/', 'core/'),
    'runtime': ('contracts/', 'runtime/', 'protocols/common/'),
    'protocols/common': ('contracts/', 'protocols/common/'),
    'protocols/demo': ('contracts/', 'protocols/common/', 'protocols/demo/'),
    'ui': ('contracts/', 'ui/', 'generated/'),
}
errors=[]
for area, allowed in RULES.items():
    for file in (ROOT/area).rglob('*'):
        if file.suffix not in ('.c','.h'): continue
        text=file.read_text(encoding='utf-8')
        for include in re.findall(r'^\s*#\s*include\s*[<"]([^>"]+)[>"]',text,re.M):
            candidate=(ROOT/include).resolve()
            if candidate.is_file():
                if not candidate.is_relative_to(ROOT) or not candidate.relative_to(ROOT).as_posix().startswith(allowed):
                    errors.append(f'{file.relative_to(ROOT)} -> {include}')
            if area in ('core','contracts') and re.search(r'lvgl|rtthread|(^|/)CO_|CANopen',include,re.I): errors.append(f'{area} imports {include}')
            if area.startswith('protocols') and ('ui/' in include or 'lvgl' in include): errors.append(f'{area} imports {include}')
            if area=='ui' and re.search(r'rtthread|rtdevice|aic_drv|aic_hal|protocols/|platform/',include,re.I): errors.append(f'UI imports {include}')
        if area=='core' and re.search(r'needle_angle|animation_progress|lv_anim',text): errors.append(f'Presentation state in {file.name}')
manifest=json.loads((ROOT/'cmake/sources.json').read_text())
# A widget owns its children through stored pointers; only the shared tree helper in
# ui/common/widgets/ itself may walk children by position.
for file in (ROOT/'ui/common/widgets').glob('*/*.c'):
    if re.search(r'lv_obj_get_child\s*\(', file.read_text(encoding='utf-8')):
        errors.append(f'{file.relative_to(ROOT)} styles a child by LVGL index instead of a stored pointer')
# Product wording and product font symbols belong to the product; common widgets render
# tags and font roles they cannot name.
for file in (ROOT/'ui/common').rglob('*'):
    if file.suffix not in ('.c','.h'): continue
    text=file.read_text(encoding='utf-8')
    for word in ('FIELD','Dashboard','Monitor','Faults','Settings'):
        if re.search(r'\b'+word+r'\b', text):
            errors.append(f'{file.relative_to(ROOT)} carries product wording: {word}')
    # Generated font symbols carry a size suffix, so a word-bounded match would let
    # meter_demo_cjk_14 through.
    for symbol in set(re.findall(r'meter_demo_cjk\w*', text)):
        errors.append(f'{file.relative_to(ROOT)} carries a product font symbol: {symbol}')
for file in (ROOT/'ui/common').rglob('*.c'):
    if 'demo' in file.name.casefold():
        errors.append(f'{file.relative_to(ROOT)} is a product source inside common')
for group in ('ui_common','ui_math'):
    for file in manifest[group]:
        if re.search(r'demo', file, re.I): errors.append(f'{group} compiles a Demo source: {file}')
# lv_translation scans every registered pack for a tag, so a tag claimed twice resolves
# by registration order rather than by intent.
owners={}
for file in list((ROOT/'ui/common/i18n').glob('*.c')) + list((ROOT/'ui/products').glob('*/*_i18n.c')):
    for block in re.findall(r'tags\[\]\s*=\s*\{(.*?)\n\};', file.read_text(encoding='utf-8'), re.S):
        for tag in re.findall(r'"([^"]+)"', block):
            if tag in owners and owners[tag] != file.name:
                errors.append(f'Translation tag {tag} claimed by {owners[tag]} and {file.name}')
            owners[tag]=file.name
for group, files in manifest.items():
    assert len(files)==len(set(files)), group
    for file in files:
        if not (ROOT/file).is_file(): errors.append(f'Missing build source {file}')
core_sources=manifest['core']
assert all(p.startswith('core/') for p in core_sources)
cmake=(ROOT/'CMakeLists.txt').read_text()
for deps in re.findall(r'target_link_libraries\(meter_core\s+([^)]*)\)',cmake,re.S):
    if any(d not in ('PUBLIC','PRIVATE','INTERFACE','m','meter_contracts') for d in deps.split()): errors.append('Core target imports an implementation dependency')
if re.search(r'\b(?:GLOB|Glob)\s*\(',cmake): errors.append('CMake uses unselected glob sources')
if errors: raise SystemExit('\n'.join(errors))
print('Architecture and selected source closure PASS')

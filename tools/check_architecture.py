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

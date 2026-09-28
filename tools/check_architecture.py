#!/usr/bin/env python3
"""强制校验第一方源码的依赖方向，而不只是记录约定。"""
from pathlib import Path
import argparse
import json
import re
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--root', type=Path, default=Path(__file__).resolve().parents[1])
ROOT = parser.parse_args().root.resolve()
RULES = {
    'contracts': ('contracts/',),
    'core': ('contracts/', 'core/', 'diagnostics/'),
    'diagnostics': ('contracts/', 'diagnostics/'),
    'runtime': ('contracts/', 'runtime/', 'protocols/common/', 'diagnostics/'),
    'protocols/common': ('contracts/', 'protocols/common/'),
    # 产品协议解析的是产品身份，而这些身份现在位于自动生成的目录中，不再放在公共域头文件里。
    'products/demo/protocol': ('contracts/', 'protocols/common/', 'products/demo/protocol/', 'products/demo/generated/'),
    'ui': ('contracts/', 'ui/', 'generated/'),
}
errors=[]
for area, allowed in RULES.items():
    for file in (ROOT/area).rglob('*'):
        if file.suffix not in ('.c','.h'): continue
        text=file.read_text(encoding='utf-8')
        for delimiter, include in re.findall(r'^\s*#\s*include\s*([<"])([^>"]+)[>"]',text,re.M):
            # Quoted includes search beside the source first, matching C lookup.
            # Product-local roots are known for the selected Demo protocol scope.
            search_roots = [file.parent] if delimiter == '"' else []
            search_roots.append(ROOT)
            if area == 'products/demo/protocol':
                search_roots.append(ROOT / 'products/demo')
            candidate = next(((base / include).resolve() for base in search_roots
                              if (base / include).is_file()), (ROOT / include).resolve())
            if candidate.is_file():
                if not candidate.is_relative_to(ROOT) or not candidate.relative_to(ROOT).as_posix().startswith(allowed):
                    errors.append(f'{file.relative_to(ROOT).as_posix()} -> {include}')
            if area in ('core','contracts','diagnostics') and re.search(r'lvgl|rtthread|rtdevice|ulog|finsh|uart|ArtInChip|aic_|(^|/)CO_|CANopen',include,re.I): errors.append(f'{area} imports {include}')
            if (area.startswith('protocols') or area.endswith('/protocol')) and ('ui/' in include or 'lvgl' in include): errors.append(f'{area} imports {include}')
            if area=='ui' and re.search(r'rtthread|rtdevice|aic_drv|aic_hal|protocols/|platform/',include,re.I): errors.append(f'UI imports {include}')
        if area=='core' and re.search(r'needle_angle|animation_progress|lv_anim',text): errors.append(f'Presentation state in {file.name}')
# 诊断数据层不持有输出后端；公共层不能绕过平台直接使用日志或 Shell。
for area in ('diagnostics', 'contracts', 'core', 'runtime', 'protocols/common', 'ui/common'):
    for file in (ROOT/area).rglob('*'):
        if file.suffix not in ('.c', '.h'): continue
        content=file.read_text(encoding='utf-8')
        backend = r'ulog|finsh|rtthread|rtdevice|aic_|uart' if area == 'ui/common' else r'ulog|finsh|rtthread|rtdevice|lvgl|aic_|uart'
        if re.search(r'^\s*#\s*include\s*[<"].*(?:' + backend + ')',content,re.M|re.I):
            errors.append(f'{file.relative_to(ROOT)} imports a platform diagnostics backend')
for area in ('protocols', 'core', 'diagnostics', 'examples/reference-mixed/canopen', 'examples/reference-mixed/services'):
    for file in (ROOT/area).rglob('*'):
        if file.suffix in ('.c', '.h') and re.search(r'\b(?:printf|rt_kprintf|snprintf|sprintf)\s*\(',file.read_text(encoding='utf-8')):
            errors.append(f'{file.relative_to(ROOT)} formats output in a hot-path layer')
manifest=json.loads((ROOT/'cmake/sources.json').read_text(encoding='utf-8'))
# 控件通过保存的指针管理自己的子对象；只有 ui/common/widgets/ 内的共享树助手可以按位置遍历子对象。
for file in (ROOT/'ui/common/widgets').glob('*/*.c'):
    if re.search(r'lv_obj_get_child\s*\(', file.read_text(encoding='utf-8')):
        errors.append(f'{file.relative_to(ROOT)} styles a child by LVGL index instead of a stored pointer')
# 产品文案与产品字体符号属于产品层；公共控件只渲染它无法命名的标签和字体角色。
for file in (ROOT/'ui/common').rglob('*'):
    if file.suffix not in ('.c','.h'): continue
    text=file.read_text(encoding='utf-8')
    for word in ('FIELD','Dashboard','Monitor','Faults','Settings'):
        if re.search(r'\b'+word+r'\b', text):
            errors.append(f'{file.relative_to(ROOT)} carries product wording: {word}')
    # 自动生成的字体符号带字号后缀，按整词匹配会漏掉 meter_demo_cjk_14。
    for symbol in set(re.findall(r'meter_demo_cjk\w*', text)):
        errors.append(f'{file.relative_to(ROOT)} carries a product font symbol: {symbol}')
for file in (ROOT/'ui/common').rglob('*.c'):
    if 'demo' in file.name.casefold():
        errors.append(f'{file.relative_to(ROOT)} is a product source inside common')
for group in ('ui_common','ui_math'):
    for file in manifest[group]:
        if re.search(r'demo', file, re.I): errors.append(f'{group} compiles a Demo source: {file}')
# 产品身份属于产品词汇表。共享控件一旦写死某个身份，就等于替之后所有产品决定了它的含义。
identities=re.findall(r'^\s+([A-Z][A-Z0-9_]+)\s*=\s*\d+,', (ROOT/'products/demo/generated/demo_catalog.h').read_text(encoding='utf-8'), re.M)
for file in (ROOT/'ui/common').rglob('*'):
    if file.suffix not in ('.c','.h'): continue
    text=file.read_text(encoding='utf-8')
    for symbol in identities:
        if re.search(r'\b'+symbol+r'\b', text): errors.append(f'{file.relative_to(ROOT)} names a product identity: {symbol}')
# 公共代码不得替产品设定上限：容量宏、定长域数组、单个故障位字，都是把某一个产品的规模写进所有产品的三种写法。
for area in ('contracts','core','runtime'):
    for file in (ROOT/area).rglob('*'):
        if file.suffix not in ('.c','.h'): continue
        text=file.read_text(encoding='utf-8')
        for macro in re.findall(r'#\s*define\s+(METER_[A-Z0-9_]+)', text):
            if re.search(r'SIGNAL|PARAMETER|MONITOR|FAULT', macro) and re.search(r'CAPACITY|COUNT|SLOTS|SIZE', macro):
                errors.append(f'{file.relative_to(ROOT)} caps a product table: {macro}')
        if re.search(r'\bmeter_(?:value_t|fault_state_t)\s+\w+\s*\[', text):
            errors.append(f'{file.relative_to(ROOT)} declares fixed-size domain storage')
        if re.search(r'\bactive_faults\b', text):
            errors.append(f'{file.relative_to(ROOT)} folds fault state into one word')
# 域、运行时、产品与板级适配代码绑定调用方提供的存储，不申请堆。
for area in ('diagnostics','contracts','core','runtime','protocols','products','platform/rtthread'):
    for file in (ROOT/area).rglob('*'):
        if file.suffix not in ('.c','.h'): continue
        for call in re.findall(r'\b(?:malloc|calloc|realloc|rt_[a-z_]*malloc|pvPortMalloc|free)\s*\(', file.read_text(encoding='utf-8')):
            errors.append(f'{file.relative_to(ROOT)} takes heap in {area}: {call}')
# 身份是句柄而不是下标：产品一旦插入或追加条目，用身份去索引域存储就会读到另一条数据。
for area in ('ui','products','protocols','platform'):
    for file in (ROOT/area).rglob('*.c'):
        for match in re.findall(r'snapshot(?:\.|->)\w+\s*\[\s*(?:METER|DEMO)_[A-Z0-9_]+', file.read_text(encoding='utf-8')):
            errors.append(f'{file.relative_to(ROOT)} indexes domain storage by identity: {match}')
# lv_translation 会遍历所有已注册的翻译包查找标签，被重复声明的标签将按注册顺序而非设计意图命中。
owners={}
for file in list((ROOT/'ui/common/i18n').glob('*.c')) + list((ROOT/'products').glob('*/ui/*_i18n.c')):
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
cmake=(ROOT/'CMakeLists.txt').read_text(encoding='utf-8')
for deps in re.findall(r'target_link_libraries\(meter_core\s+([^)]*)\)',cmake,re.S):
    if any(d not in ('PUBLIC','PRIVATE','INTERFACE','m','meter_contracts','meter_diagnostics') for d in deps.split()): errors.append('Core target imports an implementation dependency')
if re.search(r'\b(?:GLOB|Glob)\s*\(',cmake): errors.append('CMake uses unselected glob sources')
if errors: raise SystemExit('\n'.join(errors))
print('Architecture and selected source closure PASS')

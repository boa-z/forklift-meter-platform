#!/usr/bin/env python3
"""强制校验第一方源码的依赖方向，而不只是记录约定。"""
from pathlib import Path
import argparse
import json
import re
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--root', type=Path, default=Path(__file__).resolve().parents[1])
ROOT = parser.parse_args().root.resolve()
# 框架自身的规则不认识任何具体 Product；products/ 下已检出的 Product 按同一套分层规则检查。
PRODUCTS = sorted(p.parent.parent.relative_to(ROOT).as_posix()
                  for p in (ROOT / 'products').glob('*/product/sources.json'))
RULES = {
    'contracts': ('contracts/',),
    'core': ('contracts/', 'core/', 'diagnostics/'),
    'diagnostics': ('contracts/', 'diagnostics/'),
    'runtime': ('contracts/', 'runtime/', 'protocols/common/', 'diagnostics/', 'core/'),
    'protocols/common': ('contracts/', 'protocols/common/'),
    'ui': ('contracts/', 'ui/', 'generated/'),
}
for product in PRODUCTS:
    # Product 内部如何分层由 Product 自己决定；框架只限制它能触及哪些框架目录。
    own = (product + '/',)
    RULES.update({
        product + '/ui': ('contracts/', 'ui/common/') + own,
        product + '/application': ('contracts/',) + own,
        product + '/protocol': ('contracts/', 'protocols/common/') + own,
    })
# Product 内的子模块（嵌套 Git 检出）是原样引入的第三方代码，不按第一方规则检查。
VENDORED = [git.parent for git in (ROOT / 'products').glob('*/**/.git') if git.parent.parent != ROOT / 'products']
def first_party(file):
    return not any(file.is_relative_to(vendored) for vendored in VENDORED)
errors=[]
for area, allowed in RULES.items():
    product_root = next((ROOT / p for p in PRODUCTS if area.startswith(p + '/')), None)
    for file in (ROOT/area).rglob('*'):
        if file.suffix not in ('.c','.h'): continue
        text=file.read_text(encoding='utf-8')
        for delimiter, include in re.findall(r'^\s*#\s*include\s*([<"])([^>"]+)[>"]',text,re.M):
            # 带引号的包含先在源文件旁查找，与 C 查找顺序一致；Product 源码还可从其根目录包含。
            search_roots = [file.parent] if delimiter == '"' else []
            search_roots.append(ROOT)
            if product_root is not None:
                search_roots.append(product_root)
            candidate = next(((base / include).resolve() for base in search_roots
                              if (base / include).is_file()), (ROOT / include).resolve())
            if candidate.is_file():
                if not candidate.is_relative_to(ROOT) or not candidate.relative_to(ROOT).as_posix().startswith(allowed):
                    errors.append(f'{file.relative_to(ROOT).as_posix()} -> {include}')
            if area in ('core','contracts','diagnostics') and re.search(r'lvgl|rtthread|rtdevice|ulog|finsh|uart|ArtInChip|aic_|(^|/)CO_|CANopen',include,re.I): errors.append(f'{area} imports {include}')
            if (area.startswith('protocols') or area.endswith('/protocol')) and ('ui/' in include or 'lvgl' in include): errors.append(f'{area} imports {include}')
            if (area == 'ui' or area.endswith('/ui')) and re.search(r'rtthread|rtdevice|aic_drv|aic_hal|protocols/|platform/',include,re.I): errors.append(f'UI imports {include}')
        if area=='core' and re.search(r'needle_angle|animation_progress|lv_anim',text): errors.append(f'Presentation state in {file.name}')
# 诊断数据层不持有输出后端；公共层不能绕过平台直接使用日志或 Shell。
for area in ('diagnostics', 'contracts', 'core', 'runtime', 'protocols/common', 'ui/common'):
    for file in (ROOT/area).rglob('*'):
        if file.suffix not in ('.c', '.h'): continue
        content=file.read_text(encoding='utf-8')
        backend = r'ulog|finsh|rtthread|rtdevice|aic_|uart' if area == 'ui/common' else r'ulog|finsh|rtthread|rtdevice|lvgl|aic_|uart'
        if re.search(r'^\s*#\s*include\s*[<"].*(?:' + backend + ')',content,re.M|re.I):
            errors.append(f'{file.relative_to(ROOT)} imports a platform diagnostics backend')
for area in ('protocols', 'core', 'diagnostics'):
    for file in (ROOT/area).rglob('*'):
        if file.suffix in ('.c', '.h') and re.search(r'\b(?:printf|rt_kprintf|snprintf|sprintf)\s*\(',file.read_text(encoding='utf-8')):
            errors.append(f'{file.relative_to(ROOT)} formats output in a hot-path layer')
manifest=json.loads((ROOT/'cmake/sources.json').read_text(encoding='utf-8'))
# 框架是多 Product 的运行平台而不是仪表控件库：ui/common 只有翻译服务和显示调试测试卡，可视控件属于 Product。
for entry in (ROOT/'ui/common').iterdir():
    if entry.name not in ('i18n', 'testcard'):
        errors.append(f'ui/common/{entry.name} is not a shared capability (visual widgets belong to Products)')
# 产品文案与产品字体符号属于产品层。
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
for group in ('ui_common',):
    for file in manifest[group]:
        if re.search(r'demo', file, re.I): errors.append(f'{group} compiles a Demo source: {file}')
# 产品身份属于产品词汇表。共享控件一旦写死某个身份，就等于替之后所有产品决定了它的含义。
identities=[]
for product in PRODUCTS:
    for catalog in (ROOT/product/'generated').glob('*catalog.h'):
        identities += re.findall(r'^\s+([A-Z][A-Z0-9_]+)\s*=\s*\d+,', catalog.read_text(encoding='utf-8'), re.M)
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
        # 参数类型和借用指针属于框架；具体远端参数及本地设置目录只属于 Product。
        # 去掉注释和字符串，避免文档示例误报；此检查不替代宏展开和类型别名评审。
        declarations = re.sub(r'/\*.*?\*/|//[^\n]*|"(?:\\.|[^"\\])*"', '', text, flags=re.S)
        if re.search(
                r'\bmeter_parameter_(?:definition|def)_t\s+(?:const\s+)?\w+\s*'
                r'(?:\[[^;{}\]]*\]\s*(?:=|;)|=\s*\{)', declarations):
            errors.append(f'{file.relative_to(ROOT)} defines a Product parameter catalog in Framework')
# 域、运行时、产品与板级适配代码绑定调用方提供的存储，不申请堆。
for area in ('diagnostics','contracts','core','runtime','protocols','products','platform/rtthread'):
    for file in (ROOT/area).rglob('*'):
        if file.suffix not in ('.c','.h') or not first_party(file): continue
        for call in re.findall(r'\b(?:malloc|calloc|realloc|rt_[a-z_]*malloc|pvPortMalloc|free)\s*\(', file.read_text(encoding='utf-8')):
            errors.append(f'{file.relative_to(ROOT)} takes heap in {area}: {call}')
# 身份是句柄而不是下标：产品一旦插入或追加条目，用身份去索引域存储就会读到另一条数据。
for area in ('ui','products','protocols','platform'):
    for file in filter(first_party, (ROOT/area).rglob('*.c')):
        for match in re.findall(r'snapshot(?:\.|->)\w+\s*\[\s*(?:METER|DEMO)_[A-Z0-9_]+', file.read_text(encoding='utf-8')):
            errors.append(f'{file.relative_to(ROOT)} indexes domain storage by identity: {match}')
# 一个 Product 只有一个 lv_i18n 语言包，键在生成它的 JSON 里唯一（generate_i18n.py 拒绝重复键），
# 不再有多个翻译包按注册顺序互相覆盖的问题。
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
for file in (ROOT/'products').glob('*/application/*'):
    if file.suffix in ('.c', '.h') and re.search(r'lvgl|lv_obj_t|rtthread', file.read_text(encoding='utf-8'), re.I):
        # 注释可提及禁止的依赖；此处只检查实际包含的头文件及类型。
        if re.search(r'^\s*#\s*include.*(?:lvgl|rtthread)|\blv_obj_t\b', file.read_text(encoding='utf-8'), re.M):
            errors.append(f'{file.relative_to(ROOT)} imports rendering or OS types')
if errors: raise SystemExit('\n'.join(errors))
print('Architecture and selected source closure PASS')

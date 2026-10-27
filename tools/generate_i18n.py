#!/usr/bin/env python3
"""Product 语言资源 -> 框架翻译服务的静态语言表与翻译包（无 Node、无第三方依赖）。

输入是 Product 的语言清单（默认 <product>/i18n/languages.json）与每种语言一份扁平译文
（<code>.json：键 -> 文字）。源语言的译文决定键集合；语言编号就是持久化的语言字节，清单显式
写出，生成器只校验、不重排。输出一对 C 文件：meter_i18n_language_t 语言表、
lv_translation_add_static 版式的翻译包、各语言自称的键，以及（声明了 fonts 时）各语言的字体文件。

检查：编号连续且唯一、键集合一致、占位符 {0}..{9} 与源语言一致、文字非空且不含控制字符、
complete=false 的语言缺译文时用源语言占位并在 --report 中列出、声明了字体文件时逐字检查字形覆盖
（语言字体缺的字由默认字体补，与运行时的回退链一致；阿拉伯文按 LVGL 字形变换后的呈现形检查）、
可选地检查源码里 meter_i18n_tr("...") 的字面键存在。
"""
import argparse
import json
import re
import struct
import sys
import unicodedata
from pathlib import Path

FRAMEWORK = Path(__file__).resolve().parents[1]
LVGL_AP = FRAMEWORK / 'third_party/lvgl/src/misc/lv_text_ap.c'
PLURALS = {'one_other': 'METER_PLURAL_ONE_OTHER', 'other_only': 'METER_PLURAL_OTHER_ONLY'}
KEY = re.compile(r'^[A-Za-z0-9_]+(?:(?:\.|::)[A-Za-z0-9_]+)*$')
SYMBOL = re.compile(r'^[a-z][a-z0-9_]*$')
PLACEHOLDER = re.compile(r'(?<!\{)\{(\d)\}')
LITERAL_USE = re.compile(r'\bmeter_i18n_(?:tr|find)\(\s*"([^"\\]+)"\s*\)')
LANGUAGE_FIELDS = {'id', 'code', 'name_key', 'plural', 'complete', 'enabled', 'fonts', 'comment'}
MANIFEST_FIELDS = {'comment', 'symbol', 'source', 'default', 'translations', 'output', 'font_dir', 'fonts',
                   'symbol_roles', 'languages', 'sources'}


class I18nError(Exception):
    pass


def fail(message):
    raise I18nError(message)


def load_json(path):
    try:
        return json.loads(path.read_text(encoding='utf-8'))
    except (OSError, ValueError) as error:
        fail(f'{path}: {error}')


def placeholders(text):
    return sorted(set(PLACEHOLDER.findall(text)))


def check_text(where, text):
    if not isinstance(text, str) or not text.strip():
        fail(f'{where}: 文字为空或不是字符串')
    if any(unicodedata.category(c) in ('Cc', 'Cs', 'Co') and c not in '\n' for c in text) or '\ufffd' in text:
        fail(f'{where}: 含控制字符或替换字符')


def load_manifest(path):
    manifest = load_json(path)
    unknown = set(manifest) - MANIFEST_FIELDS
    if unknown:
        fail(f'{path}: 不支持的字段 {sorted(unknown)}')
    if not SYMBOL.match(str(manifest.get('symbol', ''))):
        fail(f'{path}: symbol 必须是小写 C 标识符')
    languages = manifest.get('languages')
    if not isinstance(languages, list) or not languages:
        fail(f'{path}: languages 为空')
    codes, enabled = set(), []
    for item in languages:
        if set(item) - LANGUAGE_FIELDS:
            fail(f'{path}: 语言 {item.get("code")} 不支持字段 {sorted(set(item) - LANGUAGE_FIELDS)}')
        code = item.get('code')
        if not isinstance(code, str) or not code or code in codes:
            fail(f'{path}: 语言码缺失或重复：{code}')
        codes.add(code)
        if item.get('plural', 'other_only') not in PLURALS:
            fail(f'{path}: {code} 的 plural 只能是 {sorted(PLURALS)}')
        if item.get('enabled', True):
            enabled.append(item)
        elif 'id' in item:
            fail(f'{path}: 未启用的 {code} 不能占用语言编号')
    ids = [item.get('id') for item in enabled]
    if ids != list(range(len(enabled))):
        fail(f'{path}: 启用语言的 id 必须按顺序写成 0..{len(enabled) - 1}（id 是持久化的语言字节，不能重排）')
    by_code = {item['code']: item for item in languages}
    for field in ('source', 'default'):
        code = manifest.get(field, manifest.get('source'))
        if code not in by_code or not by_code[code].get('enabled', True):
            fail(f'{path}: {field} 必须是一种启用的语言')
    if not by_code[manifest['source']].get('complete', True):
        fail(f'{path}: 源语言必须 complete')
    return manifest, enabled


def load_translations(directory, code):
    path = directory / f'{code}.json'
    table = load_json(path)
    if not isinstance(table, dict):
        fail(f'{path}: 需要 键 -> 文字 的对象')
    return table


def resolve(manifest_path, manifest, enabled):
    base = manifest_path.parent
    directory = base / manifest.get('translations', 'translations')
    source = load_translations(directory, manifest['source'])
    for key, text in source.items():
        if not KEY.match(key):
            fail(f'{manifest["source"]}.json: 键 {key!r} 只能由字母、数字、_ 与 . 或 :: 分隔')
        check_text(f'{manifest["source"]}/{key}', text)
    keys = list(source)
    report, columns = {}, {}
    for item in manifest['languages']:
        code = item['code']
        table = source if code == manifest['source'] else load_translations(directory, code)
        unknown = [key for key in table if key not in source]
        if unknown:
            fail(f'{code}.json: 源语言没有的键 {unknown[:5]}')
        missing = [key for key in keys if key not in table]
        if missing and item.get('complete', True):
            fail(f'{code}.json: 缺 {len(missing)} 条译文，如 {missing[:5]}（草稿语言请写 complete: false）')
        column = []
        for key in keys:
            if key in table:
                check_text(f'{code}/{key}', table[key])
                if placeholders(table[key]) != placeholders(source[key]):
                    fail(f'{code}/{key}: 占位符 {placeholders(table[key])} 与源语言 {placeholders(source[key])} 不同')
                column.append(table[key])
            else:
                column.append(source[key])
        report[code] = missing
        columns[code] = column
    for item in enabled:
        if 'name_key' in item and item['name_key'] not in source:
            fail(f'{item["code"]}: name_key {item["name_key"]} 不是消息键')
    return keys, columns, report


def font_roles(manifest, enabled):
    defaults = manifest.get('fonts') or {}
    roles = sorted(set(defaults).union(*(item.get('fonts', {}) for item in enabled)))
    if any(not SYMBOL.match(role) for role in roles):
        fail('fonts 的角色名必须是小写 C 标识符')
    missing = [role for role in roles if role not in defaults]
    if missing:
        fail(f'顶层 fonts 必须给出每个角色的默认字体：{missing}')
    return roles, defaults


def read_cmap(path):
    """sfnt 字体的 Unicode 码位集合（cmap 格式 4 与 12，足够覆盖 TTF/OTF）。"""
    data = path.read_bytes()
    tables = {data[12 + 16 * i:16 + 16 * i]: struct.unpack('>II', data[20 + 16 * i:28 + 16 * i])
              for i in range(struct.unpack('>H', data[4:6])[0])}
    if b'cmap' not in tables:
        fail(f'{path}: 没有 cmap')
    offset = tables[b'cmap'][0]
    codes = set()
    for i in range(struct.unpack('>H', data[offset + 2:offset + 4])[0]):
        platform, encoding, sub = struct.unpack('>HHI', data[offset + 4 + 8 * i:offset + 12 + 8 * i])
        start = offset + sub
        fmt = struct.unpack('>H', data[start:start + 2])[0]
        if (platform, encoding) not in ((0, 3), (0, 4), (3, 1), (3, 10)):
            continue
        if fmt == 4:
            segments = struct.unpack('>H', data[start + 6:start + 8])[0] // 2
            ends = struct.unpack(f'>{segments}H', data[start + 14:start + 14 + 2 * segments])
            begin = start + 16 + 2 * segments
            starts = struct.unpack(f'>{segments}H', data[begin:begin + 2 * segments])
            deltas = struct.unpack(f'>{segments}h', data[begin + 2 * segments:begin + 4 * segments])
            ranges = begin + 4 * segments
            for s in range(segments):
                range_offset = struct.unpack('>H', data[ranges + 2 * s:ranges + 2 * s + 2])[0]
                for code in range(starts[s], ends[s] + 1):
                    if code == 0xFFFF:
                        continue
                    if range_offset == 0:
                        glyph = (code + deltas[s]) & 0xFFFF
                    else:
                        at = ranges + 2 * s + range_offset + 2 * (code - starts[s])
                        glyph = struct.unpack('>H', data[at:at + 2])[0]
                        glyph = (glyph + deltas[s]) & 0xFFFF if glyph else 0
                    if glyph:
                        codes.add(code)
        elif fmt == 12:
            groups = struct.unpack('>I', data[start + 12:start + 16])[0]
            for g in range(groups):
                first, last, glyph = struct.unpack('>III', data[start + 16 + 12 * g:start + 28 + 12 * g])
                codes.update(range(first, last + 1) if glyph else range(first + 1, last + 1))
    return codes


def arabic_forms():
    """LVGL 的阿拉伯字形变换（LV_USE_ARABIC_PERSIAN_CHARS）：字母 -> 可能画出的呈现形。
    孤立形缺字时框架用原字母的字形补上（meter_i18n_font_isolated_forms），所以原字母也算数。"""
    if not LVGL_AP.is_file():
        return {}
    rows = re.findall(r'\{(\d+),\s*0x([0-9A-Fa-f]+),\s*(-?\d+),\s*(-?\d+),\s*(-?\d+),', LVGL_AP.read_text(encoding='utf-8'))
    forms = {}
    for offset, end, beginning, middle, isolated in rows:
        end = int(end, 16)
        forms[chr(0x0622 + int(offset))] = [chr(end + int(o)) for o in (0, int(beginning), int(middle), int(isolated))]
    return forms


def check_glyphs(manifest_path, manifest, enabled, keys, columns):
    roles, defaults = font_roles(manifest, enabled)
    if not roles:
        return
    directory = manifest_path.parent / manifest.get('font_dir', '.')
    cache = {}

    def cmap(name):
        if name not in cache:
            path = directory / name
            if not path.is_file():
                fail(f'字体文件不存在：{path}')
            cache[name] = read_cmap(path)
        return cache[name]

    forms = arabic_forms()
    # 只画数字与符号（如密码圆点）的角色不显示消息，不按消息检查。
    symbol_roles = set(manifest.get('symbol_roles', []))
    if symbol_roles - set(roles):
        fail(f'symbol_roles 不是已声明的字体角色：{sorted(symbol_roles - set(roles))}')
    for item in enabled:
        text = set(''.join(columns[item['code']])) - {'\n'}
        for role in (role for role in roles if role not in symbol_roles):
            face = item.get('fonts', {}).get(role, defaults[role])
            have = cmap(face) | cmap(defaults[role])
            lost = []
            for char in sorted(text):
                if char in forms:
                    # 不与后一字连写的字母，"词首形"就是孤立形。
                    isolated = forms[char][3]
                    lost += [f'{char}->U+{ord(f):04X}' for f in sorted(set(forms[char]))
                             if ord(f) not in have and (f != isolated or ord(char) not in have)]
                elif ord(char) not in have and unicodedata.category(char) not in ('Mn', 'Cf'):
                    lost.append(f'{char}(U+{ord(char):04X})')
            if lost:
                fail(f'{item["code"]}: {role} 字体 {face}（含默认 {defaults[role]} 回退）画不出 {" ".join(lost[:12])}')


def check_sources(product, manifest, keys):
    known = set(keys)
    for directory in manifest.get('sources', []):
        for path in sorted((product / directory).rglob('*.[ch]')):
            for key in LITERAL_USE.findall(path.read_text(encoding='utf-8', errors='replace')):
                if key not in known:
                    fail(f'{path.relative_to(product).as_posix()}: 键 {key} 不在语言资源里')


def c_string(text):
    out = text.replace('\\', '\\\\').replace('"', '\\"').replace('\n', '\\n').replace('??', '?\\?')
    return f'"{out}"'


def render(manifest_path, manifest, enabled, keys, columns):
    symbol = manifest['symbol']
    upper = symbol.upper()
    roles, defaults = font_roles(manifest, enabled)
    count = len(enabled)
    default_id = next(item['id'] for item in enabled if item['code'] == manifest.get('default', manifest['source']))
    origin = manifest_path.name
    banner = f'/* 从 {origin} 与各语言译文生成：python tools/generate_i18n.py；请勿手改。 */\n'
    header = [banner, f'#ifndef {upper}_H\n#define {upper}_H\n',
              '#include "ui/common/i18n/meter_i18n_runtime.h"\n',
              f'/* 启用的语言数；语言编号 0..{count - 1} 即持久化的语言字节。 */\n',
              f'#define {upper}_LANGUAGE_COUNT {count}u\n',
              f'/* 首次开机（没有已存设置）使用的语言。 */\n#define {upper}_DEFAULT_LANGUAGE {default_id}u\n',
              f'extern const meter_i18n_language_t {symbol}_languages[{upper}_LANGUAGE_COUNT];\n',
              f'extern const meter_i18n_pack_t {symbol}_pack;\n',
              f'/* 各语言名称的消息键（语言选择页），按语言编号。 */\n',
              f'extern const char *const {symbol}_name_keys[{upper}_LANGUAGE_COUNT];\n']
    source = [banner, f'#include "{symbol}.h"\n#include <stddef.h>\n',
              'static const char *const languages[] = {' +
              ', '.join(c_string(item['code']) for item in enabled) + ', NULL};\n',
              'static const char *const tags[] = {\n']
    source += [f'    {c_string(key)},\n' for key in keys]
    source.append('    NULL};\n/* 按键分行，每行依次是各语言（顺序同 languages）。 */\n'
                  'static const char *const translations[] = {\n')
    for row, key in enumerate(keys):
        source.append(f'    /* {key} */\n')
        source += [f'    {c_string(columns[item["code"]][row])},\n' for item in enabled]
    source.append('};\n')
    source.append(f'const meter_i18n_pack_t {symbol}_pack = {{languages, tags, translations}};\n')
    source.append(f'const meter_i18n_language_t {symbol}_languages[{upper}_LANGUAGE_COUNT] = {{\n')
    source += [f'    {{(meter_language_t){item["id"]}, {c_string(item["code"])}, NULL, NULL, '
               f'{PLURALS[item.get("plural", "other_only")]}}},\n' for item in enabled]
    source.append('};\n')
    source.append(f'const char *const {symbol}_name_keys[{upper}_LANGUAGE_COUNT] = {{\n')
    source += [f'    {c_string(item["name_key"]) if "name_key" in item else "NULL"},\n' for item in enabled]
    source.append('};\n')
    if roles:
        fields = ', '.join(f'*{role}' for role in roles)
        header += [f'/* 各语言按角色的字体文件名（相对 Product 的字体目录）；语言没写的角色用默认字体。 */\n',
                   f'typedef struct\n{{\n    const char {fields};\n}} {symbol}_fonts_t;\n',
                   f'extern const {symbol}_fonts_t {symbol}_default_fonts;\n',
                   f'extern const {symbol}_fonts_t {symbol}_fonts[{upper}_LANGUAGE_COUNT];\n']

        def faces(fonts):
            return '{' + ', '.join(c_string(fonts.get(role, defaults[role])) for role in roles) + '}'
        source.append(f'const {symbol}_fonts_t {symbol}_default_fonts = {faces(defaults)};\n')
        source.append(f'const {symbol}_fonts_t {symbol}_fonts[{upper}_LANGUAGE_COUNT] = {{\n')
        source += [f'    {faces(item.get("fonts", {}))},\n' for item in enabled]
        source.append('};\n')
    header.append('#endif\n')
    return ''.join(header), ''.join(source)


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument('--product-root', required=True, type=Path)
    parser.add_argument('--manifest', default='i18n/languages.json', help='相对 Product 根目录')
    parser.add_argument('--check', action='store_true', help='只校验，生成结果与已提交文件不同则失败')
    parser.add_argument('--report', action='store_true', help='列出草稿语言缺的译文')
    args = parser.parse_args(argv)
    product = args.product_root.resolve()
    manifest_path = product / args.manifest
    try:
        manifest, enabled = load_manifest(manifest_path)
        keys, columns, report = resolve(manifest_path, manifest, enabled)
        check_glyphs(manifest_path, manifest, enabled, keys, columns)
        check_sources(product, manifest, keys)
        header, source = render(manifest_path, manifest, enabled, keys, columns)
    except I18nError as error:
        print(f'FAIL: {error}', file=sys.stderr)
        return 1
    output = manifest_path.parent / manifest.get('output', 'generated')
    stale = []
    for name, content in ((f'{manifest["symbol"]}.h', header), (f'{manifest["symbol"]}.c', source)):
        path = output / name
        if args.check:
            current = path.read_text(encoding='utf-8').replace('\r\n', '\n') if path.is_file() else None
            if current != content:
                stale.append(path.relative_to(product).as_posix())
        else:
            output.mkdir(parents=True, exist_ok=True)
            path.write_text(content, encoding='utf-8', newline='\n')
    for code, missing in report.items():
        if missing and args.report:
            print(f'{code}: 草稿，缺 {len(missing)} 条：{", ".join(missing)}')
    if stale:
        print(f'FAIL: 生成文件需要更新：{", ".join(stale)}', file=sys.stderr)
        return 1
    drafts = sum(1 for missing in report.values() if missing)
    print(f'PASS: {len(keys)} 个消息键，{len(enabled)} 种启用语言（另 {len(manifest["languages"]) - len(enabled)} 种未启用）'
          f'{f"，{drafts} 种草稿" if drafts else ""}')
    return 0


if __name__ == '__main__':
    sys.exit(main())

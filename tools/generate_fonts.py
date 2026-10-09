#!/usr/bin/env python3
"""Build reviewed CJK subsets; normal builds and --check need no Node/network."""
import argparse
import hashlib
import json
import os
import re
from pathlib import Path
import shutil
import subprocess

PLATFORM = Path(__file__).resolve().parents[1]
ROOT = PLATFORM / "products/demo"
FONT = 'third_party/lvgl/scripts/generators/built_in_font/SourceHanSansSC-Normal.otf'
LICENSE = 'third_party/lvgl/scripts/generators/built_in_font/font_license/SourceHanSansSC/LICENSE.txt'
PIN = '80ca777e37a2b176770726a02e07a6fb79ef0b39'
VERSION = '1.5.3'

def portable_font_source(text):
    """规范生成注释中的字体路径，避免暴露本机目录并保持跨平台哈希。"""
    return re.sub(r'(?m)( \* Opts: --font ).*?( --size )',
                  lambda match: match[1] + FONT + match[2], text)

def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()

def license_text(path):
    # LVGL ships no .gitattributes, so a core.autocrlf host checks the upstream license out
    # as CRLF while our eol=lf copy stays LF; only the license text is under review.
    return path.read_bytes().replace(b'\r\n', b'\n')

def product_font_text(root, config):
    """合并显式声明的产品文字来源，避免动态目录中的字符漏出字体子集。"""
    extra = config.get('text_sources', [])
    if not isinstance(extra, list) or any(not isinstance(item, str) for item in extra):
        raise ValueError('font-config text_sources must be a list of relative paths')
    names = [config.get('translations', 'ui/demo_i18n.c'), *extra]
    root = root.resolve()
    texts = []
    for name in dict.fromkeys(names):
        path = (root / name).resolve()
        if Path(name).is_absolute() or not path.is_relative_to(root):
            raise ValueError('font text source must remain inside the Product: ' + name)
        texts.append(path.read_text(encoding='utf-8'))
    return '\n'.join(texts)

def main():
    global ROOT
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--check', action='store_true')
    parser.add_argument("--product-root", type=Path, default=ROOT)
    args = parser.parse_args()
    ROOT = args.product_root.resolve()
    config_path = ROOT / 'assets/font-config.json'
    config = json.loads(config_path.read_text(encoding='utf-8')) if config_path.exists() else {}
    text = (PLATFORM / 'ui/common/i18n/meter_i18n_runtime.c').read_text(encoding='utf-8') + product_font_text(ROOT, config)
    # 只从可执行字符串和翻译表提取字符，中文说明性注释不应改变字体资产。
    text = re.sub(r'/\*.*?\*/|//[^\r\n]*', '', text, flags=re.S)
    symbols = ''.join(sorted({c for c in text if ord(c) > 127}))
    manifest_path = ROOT / 'assets/fonts.json'
    head = subprocess.check_output(['git', '-C', str(PLATFORM / 'third_party/lvgl'), 'rev-parse', 'HEAD'], text=True).strip()
    assert head == PIN, 'Unreviewed LVGL font source revision'
    metadata = dict(name=config.get('name', 'Meter Demo CJK'), upstream='Source Han Sans SC', source=FONT,
                    commit=PIN, source_sha256=digest(PLATFORM / FONT), license='OFL-1.1',
                    license_file='assets/LICENSES/SourceHanSansSC-OFL.txt',
                    converter='lv_font_conv@' + VERSION, symbols=symbols,
                    modifications='Renamed subset; ASCII and demo translations; 14/20 px, 4 bpp', files={})
    if args.check:
        saved = json.loads(manifest_path.read_text(encoding='utf-8'))
        metadata['files'] = saved['files']
        assert metadata == saved, 'Font source or translations changed; regenerate fonts'
        assert license_text(ROOT / metadata['license_file']) == license_text(PLATFORM / LICENSE)
        for name, sha in saved['files'].items():
            assert digest(ROOT / name) == sha, 'Generated font hash mismatch: ' + name
        assert len(saved['files']) == 2
        print('Chinese font subset, source, license and hashes PASS')
        return
    npm = shutil.which('npm.cmd' if os.name == 'nt' else 'npm')
    if not npm:
        raise SystemExit('Node.js/npm required only to regenerate fonts')
    for size in (14, 20):
        filename = f"generated/{config.get('prefix', 'meter_demo_cjk')}_{size}.c"
        subprocess.run([npm, 'exec', '--yes', '--package=lv_font_conv@' + VERSION, '--',
                        'lv_font_conv', '--font', str(PLATFORM / FONT), '--size', str(size), '--bpp', '4',
                        '--format', 'lvgl', '--no-compress', '--no-prefilter',
                        '--range', '0x20-0x7e', '--symbols', symbols, '--output', filename],
                       cwd=ROOT, check=True)
        # Normalize tool output for cross-platform hashes.
        out = ROOT / filename
        out.write_text(portable_font_source(out.read_text(encoding='utf-8')).rstrip() + chr(10), encoding='utf-8', newline='\n')
        metadata['files'][filename] = digest(out)
    (ROOT / metadata['license_file']).parent.mkdir(parents=True, exist_ok=True)
    shutil.copyfile(PLATFORM / LICENSE, ROOT / metadata['license_file'])
    manifest_path.write_text(json.dumps(metadata, ensure_ascii=False, indent=2) + '\n', encoding='utf-8', newline='\n')

if __name__ == '__main__':
    main()
